/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the HyperRAM Read and Write example
* for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cycfg.h"
#include "cycfg_qspi_memslot.h"
#include "cy_retarget_io.h"
#include <string.h>

/*******************************************************************************
* Macros
*******************************************************************************/

#define TIMEOUT_MS              (1000ul)
#define SIZE_IN_BYTES           (64u)
#define DUMMY_CYCLE_COUNT       (14u)
#define BYTES_PER_LINE          (8u)

#define TEST_SECTOR_NO          (0)
#define HB_SECTOR_SIZE          (0x00040000UL)   /* 256KB */
#define TEST_SECTOR_ADDRESS     (HB_SECTOR_SIZE * TEST_SECTOR_NO)

#define SMIF_BASE               SMIF_HW
#define XIP_ADDRESS             CY_SMIF_XIP_BASE
#define LOOP_VALUE              20u

/*******************************************************************************
* Global Variables
*******************************************************************************/

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void print_array(char* message, uint8_t* buf, uint32_t size);


#ifndef SKIP_XIP_TEST
/* Sample function to execute in XIP mode incrementing the input value and return */
uint8_t executed_api(uint8_t data)
{
    data++;
    return(data);
}
#endif

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
*  This is the main function. It does...
*     1. Initializes UART for console output and SMIF for interfacing a HyperRAM.
*     2. Performs read operation before write followed by write and verifies the
*        written data by reading it back.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    uint8_t tx_buf[SIZE_IN_BYTES];
    uint8_t rx_buf[SIZE_IN_BYTES];

    uint16_t loop_count;

    cy_stc_smif_context_t smif_context = { 0 };

    cy_en_smif_status_t smif_status = CY_SMIF_BAD_PARAM;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_GPIO_Pin_FastInit(GPIO_PRT24, 2, CY_GPIO_DM_STRONG, 0, P24_2_SMIF0_SPIHB_RWDS);

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init_fc(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
        CYBSP_DEBUG_UART_CTS, CYBSP_DEBUG_UART_RTS, CY_RETARGET_IO_BAUDRATE);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("****************** "
            "HyperRAM Read and Write "
            "****************** \r\n\n");

    /* Enable global interrupts */
    __enable_irq();

    Cy_SMIF_Disable(SMIF_BASE);

    smif_status = Cy_SMIF_Init(SMIF_BASE, &SMIF_config, TIMEOUT_MS, &smif_context);

    if(smif_status != CY_SMIF_SUCCESS)
    {
        printf("\r\nCy_SMIF_Init - Fail \n\r");
        CY_ASSERT(0);
    }

    Cy_SMIF_SetMode(SMIF_BASE, CY_SMIF_NORMAL);
    Cy_SMIF_SetDataSelect(SMIF_BASE, smifMemConfigs[0]->slaveSelect, smifMemConfigs[0]->dataSelect);
    Cy_SMIF_Enable(SMIF_BASE, &smif_context);

    smifMemConfigs[0]->hbdeviceCfg->dummyCycles = DUMMY_CYCLE_COUNT;

    smif_status = Cy_SMIF_Memslot_Init(SMIF_BASE, (cy_stc_smif_block_config_t*)&smifBlockConfig, &smif_context);

    if(smif_status != CY_SMIF_SUCCESS)
    {
        printf("\r\nCy_SMIF_Memslot_Init - Fail \n\r");
        CY_ASSERT(0);
    }
    
    Cy_SMIF_SetMode(SMIF_BASE, CY_SMIF_NORMAL);

    memset(rx_buf, 0, SIZE_IN_BYTES);

    smif_status = Cy_SMIF_HyperBus_Read(SMIF_BASE,
        smifMemConfigs[0],
        CY_SMIF_HB_COUTINUOUS_BURST,
        TEST_SECTOR_ADDRESS,
        32,
        (uint16_t*)rx_buf,
        smifMemConfigs[0]->hbdeviceCfg->dummyCycles,
        false,
        true,
        &smif_context
    );

    if (smif_status != CY_SMIF_SUCCESS)
    {
        printf("\r\n1. Reading Data before write - Fail \n\r");
        CY_ASSERT(0);
    }

    printf("\r\n1. Reading Data before write - Success \n\r");

    print_array("Received Data before write", rx_buf, SIZE_IN_BYTES);
    printf("\r\n=============================================\r\n");

    /* Prepare the TX buffer */
    for (uint32_t index = 1; index < SIZE_IN_BYTES; index++)
    {
        tx_buf[index] = (uint8_t)index;
    }

    smif_status = Cy_SMIF_HyperBus_Write(SMIF_BASE,
        smifMemConfigs[0],
        CY_SMIF_HB_COUTINUOUS_BURST,
        TEST_SECTOR_ADDRESS,
        32,
        (uint16_t*)&tx_buf[0],
        CY_SMIF_HB_SRAM,
        smifMemConfigs[0]->hbdeviceCfg->dummyCycles,
        true,
        &smif_context
    );

    if (smif_status != CY_SMIF_SUCCESS)
    {
        printf("\r\n2. Writing data to memory - Fail \n\r");
        CY_ASSERT(0);
    }

    printf("\r\n2. Writing data to memory - Success \n\r");

    print_array("Written Data", tx_buf, SIZE_IN_BYTES);
    printf("\r\n=============================================\r\n");

    memset(rx_buf, 0, SIZE_IN_BYTES);

    smif_status = Cy_SMIF_HyperBus_Read(SMIF_BASE,
        smifMemConfigs[0],
        CY_SMIF_HB_COUTINUOUS_BURST,
        TEST_SECTOR_ADDRESS,
        32,
        (uint16_t*)rx_buf,
        smifMemConfigs[0]->hbdeviceCfg->dummyCycles,
        false,
        true,
        &smif_context
    );

    if (smif_status != CY_SMIF_SUCCESS)
    {
        printf("\r\n3. Reading back for verification - Fail \n\r");
        CY_ASSERT(0);
    }

    printf("\r\n3. Reading back for verification - Success \n\r");

    print_array("Received Data", rx_buf, SIZE_IN_BYTES);

    if (0u != memcmp(tx_buf, rx_buf, SIZE_IN_BYTES))
    {
        printf("\r\n==========================================================================\r\n");
        printf("\r\nRead data does not match with written data. Read/Write operation failed. \n\r");
        printf("\r\n==========================================================================\r\n");
    }
    else
    {
        printf("\r\n=============================================\r\n");
        printf("\r\nSUCCESS: Read data matches with written data!\r\n");
        printf("\r\n=============================================\r\n");
    }

    /***** XIP READ  *******/
    Cy_SMIF_SetMode(SMIF_BASE, CY_SMIF_MEMORY);

    /* If more than 1 cycle merge time accepted, there will be long CS# low duration when burst reading. */
    /* It may cause error because Low/High ratio of CLK should be around 50/50 during reading because of Memory device restriction. */

    volatile CY_SMIF_FLASHDATA* pHyperFlashBaseAddr = (CY_SMIF_FLASHDATA*)(XIP_ADDRESS);
    memset(rx_buf, 0, SIZE_IN_BYTES);

    /*Reading 1 Page data at a time*/
    memcpy(rx_buf, (void*)(pHyperFlashBaseAddr + TEST_SECTOR_ADDRESS), SIZE_IN_BYTES);

    print_array("4. XIP READ ", rx_buf, SIZE_IN_BYTES);

    /* Clearing merge timeout and disable the cache */
    Cy_SMIF_DeviceTransfer_ClearMergeTimeout(SMIF_BASE, smifMemConfigs[0]->slaveSelect);
    Cy_SMIF_CacheInvalidate(SMIF_BASE, CY_SMIF_CACHE_BOTH);
    Cy_SMIF_CacheDisable(SMIF_BASE, CY_SMIF_CACHE_BOTH);

    /* Put the device in XIP mode */
    printf("\n\rVerify execution from memory in XIP Mode\n\r");
    printf("--------------------------------------------\n\r");
    Cy_SMIF_SetMode(SMIF_BASE, CY_SMIF_MEMORY);

    loop_count = executed_api(LOOP_VALUE);

    if (loop_count == LOOP_VALUE + 1)
    {
        printf("XIP Read Functionality - Success\n\r");
    }
    else
    {
        printf("XIP Read Functionality - Fail\n\r");
    }

    printf("\n\rCompleted SMIF HyperRAM Test app verification\n\r");


    for (;;)
    {
    }
}

/*******************************************************************************
* Function Name: print_array
********************************************************************************
* Summary:
*  Prints the content of the buffer to the UART console.
*
* Parameters:
*  message - message to print before array output
*  buf - buffer to print on the console.
*  size - size of the buffer.
*
* Return:
*  void
*
*******************************************************************************/
void print_array(char* message, uint8_t* buf, uint32_t size)
{
    printf("\n\r%s (%u bytes):\n\r", message, (unsigned int)size);
    printf("-------------------------\r\n");

    for (uint32_t index = 0; index < size; index++)
    {
        printf("0x%02X ", buf[index]);

        if (0u == ((index + 1) % BYTES_PER_LINE))
        {
            printf("\r\n");
        }
    }
}

/* [] END OF FILE */
