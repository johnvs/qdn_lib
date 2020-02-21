/**************************************************************************
 *
 * Copyright (c) 2013, Qromodyn Corporation
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 **************************************************************************/

#include "qdn_spi.h"
#include "qdn_gpio.h"
#include "qdn_util.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_rcc.h"
#include "qdn_stm32f10x.h"


QDN_SPI::QDN_SPI(int unit, QDN_GPIO_Output& Clk0, QDN_GPIO_Output& MOSI0, QDN_GPIO_Input& MISO0)
	: Clk(Clk0)
	, MOSI(MOSI0)
	, MISO(MISO0)
{
    SPI_StructInit(&spiInitStruct);
    spiInitStruct.SPI_CPOL      = SPI_CPOL_Low;
    spiInitStruct.SPI_CPHA      = SPI_CPHA_1Edge;
    spiInitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    spiInitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spiInitStruct.SPI_Mode      = SPI_Mode_Master;
    spiInitStruct.SPI_NSS       = SPI_NSS_Soft;
    spiInitStruct.SPI_FirstBit  = SPI_FirstBit_MSB;
   // spiInitStruct.SPI_CRCPolynomial = 7;

	switch(unit)
	{
	case 1:spi = static_cast<SpiThing*>(SPI1); break;
	case 2:spi = static_cast<SpiThing*>(SPI2); break;
	case 3:spi = static_cast<SpiThing*>(SPI3); break;
	default: QDN_Exception(); break;
	}

	Clk.SetMode(GPIO_Mode_AF_PP);
	MOSI.SetMode(GPIO_Mode_AF_PP);
	MISO.SetMode(GPIO_Mode_AF_PP);
}

QDN_SPI& QDN_SPI::SetClockPolarity(ClockPolarity state0)
{
    spiInitStruct.SPI_CPOL = (state0 == ClockPolarity::IdleHi) ? SPI_CPOL_High  : SPI_CPOL_Low;
    return *this;
}

QDN_SPI& QDN_SPI::SetClockPhase(ClockPhase phase)
{
    spiInitStruct.SPI_CPHA = (phase == ClockPhase::FirstEdge) ? SPI_CPHA_1Edge : SPI_CPHA_2Edge;
    return *this;
}

QDN_SPI& QDN_SPI::SetClockRateShift(uint32_t rightShift)
{
    uint16_t prescalarMask = (uint16_t)(rightShift - 1) << 3;
    if (!IS_SPI_BAUDRATE_PRESCALER(prescalarMask))
        QDN_Exception();

    spiInitStruct.SPI_BaudRatePrescaler = prescalarMask;
    return *this;
}

QDN_SPI& QDN_SPI::SetBitMode(uint8_t bits)
{
    if (bits !=8 && bits != 16)
        QDN_Exception("not supported");

    spiInitStruct.SPI_DataSize = (bits == 8) ? SPI_DataSize_8b : SPI_DataSize_16b;
    return *this;
}

void QDN_SPI::Enable(bool enable)
{
    if (enable)
        SPI_Cmd(spi, ENABLE);
    else
        SPI_Cmd(spi, DISABLE);
}

void QDN_SPI::SetClockPhaseImmediate(ClockPhase phase)
{
    // Clear the CPHA bit
    spi->CR1 &= ~SPI_CR1_CPHA;

    // Set CPHA bit to new value
    if (phase == ClockPhase::FirstEdge)
    {
        spi->CR1 |= SPI_CPHA_1Edge;
    } else if (phase == ClockPhase::SecondEdge)
    {
        spi->CR1 |= SPI_CPHA_2Edge;
    }
}

void QDN_SPI::Init(void)
{
	Clk.HighSpeedInit();
	MOSI.HighSpeedInit();
	MISO.HighSpeedInit();

	RCC->APB2ENR |= RCC_APB2Periph_AFIO ;

	if (spi == SPI1)      RCC->APB2ENR |= RCC_APB2Periph_SPI1;
	else if (spi == SPI2) RCC->APB1ENR |= RCC_APB1Periph_SPI2;
	else if (spi == SPI3) RCC->APB1ENR |= RCC_APB1Periph_SPI3;

    SPI_I2S_DeInit(spi);
    SPI_Init(spi, &spiInitStruct);

    SPI_Cmd(spi, ENABLE);
}

uint8_t QDN_SPI::WriteReadU8(uint8_t byte)
{
	spi->DR = byte;
	while( (spi->SR & SPI_I2S_FLAG_TXE)  == 0 ) ; // wait until TX is empty
	while( (spi->SR & SPI_I2S_FLAG_RXNE) == 0 ) ; // wait until RX is full

	return (uint8_t) spi->DR; //lint !e529
}

uint16_t QDN_SPI::WriteReadU16_LE(uint16_t word)
{
	uint16_t result = 0;
	result =  WriteReadU8(word & 0xFF);
	result |= (static_cast<uint16_t>(WriteReadU8(word >> 8)) << 8);
	return result;
}

uint16_t QDN_SPI::WriteReadU16_BE(uint16_t word)
{
	uint16_t result = 0;
	result = (static_cast<uint16_t>(WriteReadU8(word >> 8)) << 8);
	result |= WriteReadU8(word & 0xFF);
	return result;
}

void QDN_SPI::EnableDMA(void)
{
    SPI_I2S_DMACmd(spi, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE);
}

void QDN_SPI::DisableDMA(void)
{
    SPI_I2S_DMACmd(spi, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, DISABLE);
}
