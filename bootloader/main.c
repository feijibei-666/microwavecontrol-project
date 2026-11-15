#include <stdint.h>
#include <string.h>

#include "stm32f10x.h"
#include "firmware_layout.h"
#include "heater.h"
#include "key.h"

#define CHUNK_SIZE        256U
#define RX_TIMEOUT_MS     2500U
#define BOOT_HELLO        0x7EU
#define BOOT_ACK          0x06U
#define BOOT_NAK          0x15U
#define UPDATE_MAGIC      0x5055574DUL  /* MWUP */
#define UPDATE_DONE       0x454E4F44UL  /* DONE */
#define IMAGE_VALID       0xB007U
#define META_LENGTH       0U
#define META_CRC          4U
#define META_VALID        8U
#define DEMCR_REG         (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL_REG      (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT_REG    (*(volatile uint32_t *)0xE0001004UL)

extern void Boot_Jump(uint32_t initial_msp, uint32_t reset_handler);

static uint32_t cycles_per_ms;
static uint8_t packet[6U + CHUNK_SIZE + 4U];

static uint16_t GetU16(const uint8_t *data)
{
    return (uint16_t)(data[0] | (uint16_t)data[1] << 8);
}

static uint32_t GetU32(const uint8_t *data)
{
    return (uint32_t)GetU16(data) | (uint32_t)GetU16(data + 2U) << 16;
}

static uint32_t Crc32(const uint8_t *data, uint32_t count)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint8_t bit;
    while (count--)
    {
        crc ^= *data++;
        for (bit = 0; bit < 8U; bit++)
        {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
        }
    }
    return ~crc;
}

static uint8_t VectorIsValid(uint32_t size)
{
    const uint32_t *vectors = (const uint32_t *)FIRMWARE_APP_BASE;
    uint32_t reset_address = vectors[1] & ~1UL;

    return size >= 8U && size <= FIRMWARE_APP_CAPACITY &&
        vectors[0] > FIRMWARE_SRAM_BASE && vectors[0] <= FIRMWARE_SRAM_TOP &&
        (vectors[0] & 7U) == 0U && (vectors[1] & 1U) &&
        reset_address >= FIRMWARE_APP_BASE + 8U &&
        reset_address < FIRMWARE_APP_BASE + size;
}

static uint8_t ImageIsValid(void)
{
    const uint8_t *meta = (const uint8_t *)FIRMWARE_METADATA_BASE;
    uint32_t size = GetU32(meta + META_LENGTH);

    return GetU16(meta + META_VALID) == IMAGE_VALID && VectorIsValid(size) &&
        Crc32((const uint8_t *)FIRMWARE_APP_BASE, size) == GetU32(meta + META_CRC);
}

static void StartApp(void)
{
    const uint32_t *vectors = (const uint32_t *)FIRMWARE_APP_BASE;
    uint32_t i;

    /* 只从复位路径进入这里。清理中断后交接向量表和 MSP。 */
    __disable_irq();
    SysTick->CTRL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    for (i = 0; i < 2U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }
    SCB->VTOR = FIRMWARE_APP_BASE;
    __DSB();
    __ISB();
    Boot_Jump(vectors[0], vectors[1]);
}

static uint8_t UpgradeRequested(void)
{
    GPIO_InitTypeDef gpio;
    uint32_t start;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = KEY2_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(KEY2_PORT, &gpio);
    if (KEY2_READ() != KEY_ACTIVE_LEVEL) { return 0; }
    start = DWT_CYCCNT_REG;
    while ((uint32_t)(DWT_CYCCNT_REG - start) < cycles_per_ms * 20U)
    {
        if (KEY2_READ() != KEY_ACTIVE_LEVEL) { return 0; }
    }
    return 1;
}

static void SerialInit(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    DMA_InitTypeDef dma;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&uart);
    uart.USART_BaudRate = 115200U;
    USART_Init(USART1, &uart);
    DMA_DeInit(DMA1_Channel5);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)packet;
    dma.DMA_BufferSize = sizeof(packet);
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_Init(DMA1_Channel5, &dma);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

static void SendReply(uint8_t reply)
{
    while (!(USART1->SR & USART_SR_TXE)) {}
    USART1->DR = reply;
}

static uint8_t Receive(uint8_t *data, uint16_t size)
{
    uint32_t start = DWT_CYCCNT_REG;
    uint8_t ok = 0;

    /* USART1 RX 使用 DMA1 CH5。分段等待有期限，避免断线后一直占着旧会话。 */
    DMA1->IFCR = DMA_IFCR_CGIF5;
    DMA1_Channel5->CMAR = (uint32_t)data;
    DMA1_Channel5->CNDTR = size;
    DMA1_Channel5->CCR |= DMA_CCR5_EN;
    while ((uint32_t)(DWT_CYCCNT_REG - start) < cycles_per_ms * RX_TIMEOUT_MS)
    {
        if ((USART1->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) ||
            (DMA1->ISR & DMA_ISR_TEIF5))
        {
            break;
        }
        if (DMA1->ISR & DMA_ISR_TCIF5)
        {
            ok = 1;
            break;
        }
    }
    DMA1_Channel5->CCR &= ~DMA_CCR5_EN;
    if (!ok)
    {
        /* 读取 SR 和 DR 清除 USART 接收错误，再等待握手。 */
        (void)USART1->SR;
        (void)USART1->DR;
    }
    return ok;
}

static uint8_t EraseTarget(uint32_t size)
{
    uint32_t address;
    uint8_t ok;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    /* 先清除有效标记。接收中断后复位会留在 Bootloader。 */
    ok = FLASH_ErasePage(FIRMWARE_METADATA_BASE) == FLASH_COMPLETE;
    for (address = FIRMWARE_APP_BASE; ok && address < FIRMWARE_APP_BASE + size;
         address += FIRMWARE_FLASH_PAGE_SIZE)
    {
        ok = FLASH_ErasePage(address) == FLASH_COMPLETE;
    }
    FLASH_Lock();
    return ok;
}

static uint8_t WriteChunk(uint32_t offset, const uint8_t *bytes, uint16_t count)
{
    uint32_t address = FIRMWARE_APP_BASE + offset;
    uint16_t i, halfword;
    uint8_t ok = 1;

    FLASH_Unlock();
    for (i = 0; i < count; i += 2U)
    {
        halfword = (uint16_t)(bytes[i] | (uint16_t)((i + 1U < count) ? bytes[i + 1U] : 0xFFU) << 8);
        if (halfword != 0xFFFFU && FLASH_ProgramHalfWord(address + i, halfword) != FLASH_COMPLETE)
        {
            ok = 0;
            break;
        }
    }
    FLASH_Lock();
    return ok && memcmp((const uint8_t *)address, bytes, count) == 0;
}

static uint8_t CommitImage(uint32_t size, uint32_t crc)
{
    uint8_t ok;
    if (!VectorIsValid(size) || Crc32((const uint8_t *)FIRMWARE_APP_BASE, size) != crc)
    {
        return 0;
    }

    FLASH_Unlock();
    ok = FLASH_ProgramWord(FIRMWARE_METADATA_BASE + META_LENGTH, size) == FLASH_COMPLETE &&
         FLASH_ProgramWord(FIRMWARE_METADATA_BASE + META_CRC, crc) == FLASH_COMPLETE;
    if (ok)
    {
        /* 整镜像校验通过后最后写有效标记。 */
        ok = FLASH_ProgramHalfWord(FIRMWARE_METADATA_BASE + META_VALID, IMAGE_VALID) == FLASH_COMPLETE;
    }
    FLASH_Lock();
    return ok && ImageIsValid();
}

static void ReceiveImage(void)
{
    uint32_t size, expected_crc, offset, written = 0, previous_offset = 0;
    uint16_t count, previous_count = 0;

    SendReply(BOOT_ACK);
    if (!Receive(packet, 12U) || GetU32(packet) != UPDATE_MAGIC)
    {
        SendReply(BOOT_NAK);
        return;
    }
    size = GetU32(packet + 4U);
    expected_crc = GetU32(packet + 8U);
    if (size < 8U || size > FIRMWARE_APP_CAPACITY || !EraseTarget(size))
    {
        SendReply(BOOT_NAK);
        return;
    }
    SendReply(BOOT_ACK);

    while (Receive(packet, 4U))
    {
        offset = GetU32(packet);
        if (offset == UPDATE_DONE)
        {
            SendReply(written == size && CommitImage(size, expected_crc) ? BOOT_ACK : BOOT_NAK);
            return;
        }
        if (!Receive(packet + 4U, 2U)) { return; }
        count = GetU16(packet + 4U);
        if (count == 0U || count > CHUNK_SIZE || !Receive(packet + 6U, count + 4U))
        {
            return;
        }
        if (Crc32(packet, 6U + count) != GetU32(packet + 6U + count))
        {
            SendReply(BOOT_NAK);
            continue;
        }
        if (previous_count && offset == previous_offset && count == previous_count &&
            memcmp((const uint8_t *)(FIRMWARE_APP_BASE + offset), packet + 6U, count) == 0)
        {
            SendReply(BOOT_ACK);
            continue;
        }
        if (offset != written || count > size - written ||
            (count != CHUNK_SIZE && count != size - written))
        {
            SendReply(BOOT_NAK);
            continue;
        }
        if (!WriteChunk(offset, packet + 6U, count))
        {
            /* Flash 写失败需重新擦除，不把它当作单包 CRC 错误。 */
            return;
        }
        previous_offset = offset;
        previous_count = count;
        written += count;
        SendReply(BOOT_ACK);
    }
}

int main(void)
{
    uint8_t byte;

    Heater_Off();
    SystemCoreClockUpdate();
    cycles_per_ms = SystemCoreClock / 1000U;
    DEMCR_REG |= 1UL << 24;
    DWT_CTRL_REG |= 1U;
    if (!UpgradeRequested() && ImageIsValid())
    {
        StartApp();
    }

    SerialInit();
    for (;;)
    {
        /* 升级后继续等待串口，松开 KEY2 并复位才启动新 APP。 */
        if (Receive(&byte, 1U) && byte == BOOT_HELLO)
        {
            ReceiveImage();
        }
    }
}
