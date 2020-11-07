EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 1 4
Title "Printalyzer - Main Board"
Date ""
Rev "?"
Comp "LogicProbe.org"
Comment1 "Derek Konigsberg"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L MCU_ST_STM32F4:STM32F411RETx U4
U 1 1 5F8A24AA
P 3600 3250
F 0 "U4" H 3050 4900 50  0000 C CNN
F 1 "STM32F411RETx" H 4150 1500 50  0000 C CNN
F 2 "Package_QFP:LQFP-64_10x10mm_P0.5mm" H 3000 1550 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00115249.pdf" H 3600 3250 50  0001 C CNN
F 4 "IC MCU 32BIT 512KB FLASH 64LQFP" H 3600 3250 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3600 3250 50  0001 C CNN "Manufacturer"
F 6 "STM32F411RET6" H 3600 3250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3600 3250 50  0001 C CNN "Supplier"
F 8 "497-14909-ND" H 3600 3250 50  0001 C CNN "Supplier PN"
	1    3600 3250
	1    0    0    -1  
$EndComp
$Sheet
S 6550 750  1400 750 
U 5F8B674D
F0 "Power Supply and Control" 50
F1 "main-board-power.sch" 50
F2 "RELAY_ENLARGER" I L 6550 850 50 
F3 "RELAY_SAFELIGHT" I L 6550 950 50 
$EndSheet
$Sheet
S 6550 1750 1400 1350
U 5F8B69B5
F0 "User Input" 50
F1 "main-board-input.sch" 50
F2 "INT_I2C_SDA" B L 6550 1850 50 
F3 "INT_I2C_SCL" B L 6550 1950 50 
F4 "SENSOR_BTN" I L 6550 2450 50 
F5 "FOOTSWITCH" I L 6550 2550 50 
F6 "LED_SDI" I L 6550 2700 50 
F7 "LED_CLK" I L 6550 2800 50 
F8 "LED_LE" I L 6550 2900 50 
F9 "LED_~OE" I L 6550 3000 50 
F10 "KEY_~INT" O L 6550 2050 50 
F11 "ENC_A" O L 6550 2200 50 
F12 "ENC_B" O L 6550 2300 50 
$EndSheet
$Comp
L power:GND #PWR0103
U 1 1 5F8B9F00
P 9400 5050
F 0 "#PWR0103" H 9400 4800 50  0001 C CNN
F 1 "GND" H 9405 4877 50  0000 C CNN
F 2 "" H 9400 5050 50  0001 C CNN
F 3 "" H 9400 5050 50  0001 C CNN
	1    9400 5050
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR0104
U 1 1 5F8BA7DD
P 9350 2950
F 0 "#PWR0104" H 9350 2800 50  0001 C CNN
F 1 "+3.3V" H 9365 3123 50  0000 C CNN
F 2 "" H 9350 2950 50  0001 C CNN
F 3 "" H 9350 2950 50  0001 C CNN
	1    9350 2950
	1    0    0    -1  
$EndComp
Wire Wire Line
	9500 3150 9350 3150
Wire Wire Line
	9350 3150 9350 2950
NoConn ~ 9500 3250
NoConn ~ 9500 3850
NoConn ~ 9500 4450
NoConn ~ 9500 4750
Wire Wire Line
	9500 3050 9400 3050
Wire Wire Line
	9400 3050 9400 3450
Wire Wire Line
	9500 3450 9400 3450
Connection ~ 9400 3450
Wire Wire Line
	9400 3450 9400 3550
Wire Wire Line
	9500 3550 9400 3550
Connection ~ 9400 3550
Wire Wire Line
	9500 3950 9400 3950
Connection ~ 9400 3950
Wire Wire Line
	9400 3950 9400 4050
Wire Wire Line
	9500 4050 9400 4050
Connection ~ 9400 4050
Wire Wire Line
	9400 4050 9400 4150
Wire Wire Line
	9500 4150 9400 4150
Connection ~ 9400 4150
Wire Wire Line
	9400 4150 9400 4250
Wire Wire Line
	9500 4250 9400 4250
Connection ~ 9400 4250
Wire Wire Line
	9400 4250 9400 4350
Wire Wire Line
	9500 4350 9400 4350
Connection ~ 9400 4350
Wire Wire Line
	9400 4350 9400 4850
Wire Wire Line
	9500 4850 9400 4850
Connection ~ 9400 4850
Wire Wire Line
	9400 4850 9400 4950
Wire Wire Line
	9500 4950 9400 4950
Connection ~ 9400 4950
Wire Wire Line
	9400 4950 9400 5050
Wire Wire Line
	9500 3350 9250 3350
Wire Wire Line
	9500 3650 9250 3650
Wire Wire Line
	9500 4550 9250 4550
Wire Wire Line
	9500 4650 9250 4650
Text Label 9250 3350 2    50   ~ 0
DISP_D~C
Text Label 9250 3650 2    50   ~ 0
DISP_SPI_SCK
Text Label 9250 3750 2    50   ~ 0
DISP_SPI_MOSI
Text Label 9250 4550 2    50   ~ 0
DISP_~RES
Text Label 9250 4650 2    50   ~ 0
DISP_~CS
$Comp
L Project:Conn_ST_STDC14 J2
U 1 1 5FA6AC8D
P 1700 6750
F 0 "J2" H 1257 6796 50  0000 R CNN
F 1 "STDC14" H 1257 6705 50  0000 R CNN
F 2 "lib_fp:Samtec_FTSH-107-01-L-DV-K-A_2x07_P1.27mm" H 1700 5650 50  0001 C CNN
F 3 "https://www.mouser.com/datasheet/2/527/ftsh_smt-1316912.pdf" V 1350 5500 50  0001 C CNN
F 4 "CONN HEADER SMD 14POS 1.27MM" H 1700 6750 50  0001 C CNN "Description"
F 5 "Samtec Inc." H 1700 6750 50  0001 C CNN "Manufacturer"
F 6 "FTSH-107-01-L-DV-K-A" H 1700 6750 50  0001 C CNN "Manufacturer PN"
F 7 "Mouser" H 1700 6750 50  0001 C CNN "Supplier"
F 8 "200-FTSH10701LDVKA" H 1700 6750 50  0001 C CNN "Supplier PN"
	1    1700 6750
	1    0    0    -1  
$EndComp
Wire Wire Line
	9400 3550 9400 3950
Wire Wire Line
	9500 3750 9250 3750
$Comp
L Connector_Generic:Conn_01x20 J3
U 1 1 5F8B8832
P 9700 3950
F 0 "J3" H 9650 4950 50  0000 L CNN
F 1 "NHD OLED Connector" H 9500 2850 50  0000 L CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x20_P2.54mm_Vertical" H 9700 3950 50  0001 C CNN
F 3 "https://drawings-pdf.s3.amazonaws.com/10492.pdf" H 9700 3950 50  0001 C CNN
F 4 "CONN HDR 20POS 0.1 GOLD PCB" H 9700 3950 50  0001 C CNN "Description"
F 5 "Sullins Connector Solutions" H 9700 3950 50  0001 C CNN "Manufacturer"
F 6 "PPPC201LFBN-RC" H 9700 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9700 3950 50  0001 C CNN "Supplier"
F 8 "PPPC201LFBN-RC" H 9700 3950 50  0001 C CNN "Supplier PN"
	1    9700 3950
	1    0    0    -1  
$EndComp
$Comp
L Device:Crystal_GND24 Y1
U 1 1 5F94F8E6
P 1450 3200
F 0 "Y1" H 1500 3400 50  0000 L CNN
F 1 "16MHz" H 1500 3000 50  0000 L CNN
F 2 "lib_fp:Crystal_SMD_Kyocera_CX3225GB" H 1450 3200 50  0001 C CNN
F 3 "http://prdct-search.kyocera.co.jp/crystal-ic/catalog/en/cx3225gb_e.pdf" H 1450 3200 50  0001 C CNN
F 4 "CRYSTAL 16.0000MHZ 8PF SMD" H 1450 3200 50  0001 C CNN "Description"
F 5 "Kyocera International Inc." H 1450 3200 50  0001 C CNN "Manufacturer"
F 6 "CX3225GB16000D0HPQCC" H 1450 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1450 3200 50  0001 C CNN "Supplier"
F 8 "1253-1181-1-ND" H 1450 3200 50  0001 C CNN "Supplier PN"
	1    1450 3200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C21
U 1 1 5F9506A3
P 1200 3550
F 0 "C21" H 1292 3596 50  0000 L CNN
F 1 "8pF" H 1292 3505 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1200 3550 50  0001 C CNN
F 3 "https://www.yageo.com/upload/media/product/productsearch/datasheet/mlcc/UPY-GP_NP0_16V-to-50V_18.pdf" H 1200 3550 50  0001 C CNN
F 4 "CAP CER 8PF 50V NPO 0603" H 1200 3550 50  0001 C CNN "Description"
F 5 "Yageo" H 1200 3550 50  0001 C CNN "Manufacturer"
F 6 "CC0603CRNPO9BN8R0" H 1200 3550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1200 3550 50  0001 C CNN "Supplier"
F 8 "311-3864-1-ND" H 1200 3550 50  0001 C CNN "Supplier PN"
	1    1200 3550
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C22
U 1 1 5F950FD6
P 1700 3550
F 0 "C22" H 1792 3596 50  0000 L CNN
F 1 "8pF" H 1792 3505 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1700 3550 50  0001 C CNN
F 3 "https://www.yageo.com/upload/media/product/productsearch/datasheet/mlcc/UPY-GP_NP0_16V-to-50V_18.pdf" H 1700 3550 50  0001 C CNN
F 4 "CAP CER 8PF 50V NPO 0603" H 1700 3550 50  0001 C CNN "Description"
F 5 "Yageo" H 1700 3550 50  0001 C CNN "Manufacturer"
F 6 "CC0603CRNPO9BN8R0" H 1700 3550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1700 3550 50  0001 C CNN "Supplier"
F 8 "311-3864-1-ND" H 1700 3550 50  0001 C CNN "Supplier PN"
	1    1700 3550
	1    0    0    -1  
$EndComp
Wire Wire Line
	1300 3200 1200 3200
Wire Wire Line
	1200 3200 1200 3450
Wire Wire Line
	1600 3200 1700 3200
Wire Wire Line
	1700 3200 1700 3450
Wire Wire Line
	2900 2950 1700 2950
Wire Wire Line
	1700 2950 1700 3200
Connection ~ 1700 3200
Wire Wire Line
	2900 2850 1200 2850
Wire Wire Line
	1200 2850 1200 3200
Connection ~ 1200 3200
$Comp
L power:GND #PWR0105
U 1 1 5F95AD21
P 1200 3650
F 0 "#PWR0105" H 1200 3400 50  0001 C CNN
F 1 "GND" H 1205 3477 50  0000 C CNN
F 2 "" H 1200 3650 50  0001 C CNN
F 3 "" H 1200 3650 50  0001 C CNN
	1    1200 3650
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0106
U 1 1 5F95B4D9
P 1700 3650
F 0 "#PWR0106" H 1700 3400 50  0001 C CNN
F 1 "GND" H 1705 3477 50  0000 C CNN
F 2 "" H 1700 3650 50  0001 C CNN
F 3 "" H 1700 3650 50  0001 C CNN
	1    1700 3650
	1    0    0    -1  
$EndComp
NoConn ~ 1450 3400
NoConn ~ 1450 3000
$Comp
L Project:M24M01-RMN U6
U 1 1 5F950792
P 6000 5900
F 0 "U6" H 5800 6150 50  0000 C CNN
F 1 "M24M01-RMN" H 6300 5650 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 6000 6250 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/group0/cb/91/ba/7d/0b/c1/4d/f6/CD00147128/files/CD00147128.pdf/jcr:content/translations/en.CD00147128.pdf" H 6050 5400 50  0001 C CNN
F 4 "IC EEPROM 1M I2C 1MHZ 8SO" H 6000 5900 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 6000 5900 50  0001 C CNN "Manufacturer"
F 6 "M24M01-RMN6TP" H 6000 5900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 6000 5900 50  0001 C CNN "Supplier"
F 8 "497-6351-1-ND" H 6000 5900 50  0001 C CNN "Supplier PN"
	1    6000 5900
	1    0    0    -1  
$EndComp
$Comp
L Device:L_Small L3
U 1 1 5F951860
P 4650 900
F 0 "L3" V 4850 900 50  0000 C CNN
F 1 "600 Ohm" V 4750 900 50  0000 C CNN
F 2 "Inductor_SMD:L_0603_1608Metric" H 4650 900 50  0001 C CNN
F 3 "https://www.yuden.co.jp/productdata/catalog/mlci07_e.pdf" H 4650 900 50  0001 C CNN
F 4 "FERRITE BEAD 600 OHM 0603 1LN" H 4650 900 50  0001 C CNN "Description"
F 5 "Taiyo Yuden" H 4650 900 50  0001 C CNN "Manufacturer"
F 6 "BK1608HS601-T" H 4650 900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4650 900 50  0001 C CNN "Supplier"
F 8 "587-1874-1-ND" H 4650 900 50  0001 C CNN "Supplier PN"
	1    4650 900 
	0    -1   -1   0   
$EndComp
$Comp
L power:+3.3V #PWR0107
U 1 1 5F96416A
P 4950 900
F 0 "#PWR0107" H 4950 750 50  0001 C CNN
F 1 "+3.3V" H 4965 1073 50  0000 C CNN
F 2 "" H 4950 900 50  0001 C CNN
F 3 "" H 4950 900 50  0001 C CNN
	1    4950 900 
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C14
U 1 1 5F9656F7
P 4450 1250
F 0 "C14" H 4542 1296 50  0000 L CNN
F 1 "0.1uF" H 4542 1205 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4450 1250 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 4450 1250 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 4450 1250 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 4450 1250 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 4450 1250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4450 1250 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 4450 1250 50  0001 C CNN "Supplier PN"
	1    4450 1250
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C15
U 1 1 5F965D4A
P 4850 1250
F 0 "C15" H 4942 1296 50  0000 L CNN
F 1 "1uF" H 4942 1205 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4850 1250 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 4850 1250 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 4850 1250 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 4850 1250 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 4850 1250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4850 1250 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 4850 1250 50  0001 C CNN "Supplier PN"
	1    4850 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	3900 1550 3900 1050
Wire Wire Line
	3900 1050 4450 1050
Wire Wire Line
	4450 1150 4450 1050
Connection ~ 4450 1050
Wire Wire Line
	4450 1050 4850 1050
Wire Wire Line
	4850 1150 4850 1050
$Comp
L power:GND #PWR0108
U 1 1 5F969C61
P 4450 1400
F 0 "#PWR0108" H 4450 1150 50  0001 C CNN
F 1 "GND" H 4455 1227 50  0000 C CNN
F 2 "" H 4450 1400 50  0001 C CNN
F 3 "" H 4450 1400 50  0001 C CNN
	1    4450 1400
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 1350 4450 1400
Wire Wire Line
	4850 1350 4850 1400
Wire Wire Line
	4850 1400 4450 1400
Connection ~ 4450 1400
Wire Wire Line
	4750 900  4950 900 
Wire Wire Line
	4550 900  4450 900 
Wire Wire Line
	4450 900  4450 1050
$Comp
L Device:C_Small C13
U 1 1 5F97FD14
P 3000 1200
F 0 "C13" H 3092 1246 50  0000 L CNN
F 1 "0.1uF" H 3092 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 3000 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 3000 1200 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 3000 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 3000 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 3000 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3000 1200 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 3000 1200 50  0001 C CNN "Supplier PN"
	1    3000 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C12
U 1 1 5F980A35
P 2600 1200
F 0 "C12" H 2692 1246 50  0000 L CNN
F 1 "0.1uF" H 2692 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2600 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 2600 1200 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 2600 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 2600 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 2600 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2600 1200 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 2600 1200 50  0001 C CNN "Supplier PN"
	1    2600 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C11
U 1 1 5F980D6F
P 2200 1200
F 0 "C11" H 2292 1246 50  0000 L CNN
F 1 "0.1uF" H 2292 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2200 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 2200 1200 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 2200 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 2200 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 2200 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2200 1200 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 2200 1200 50  0001 C CNN "Supplier PN"
	1    2200 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C10
U 1 1 5F9811DA
P 1800 1200
F 0 "C10" H 1892 1246 50  0000 L CNN
F 1 "0.1uF" H 1892 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1800 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1800 1200 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1800 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1800 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1800 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1800 1200 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1800 1200 50  0001 C CNN "Supplier PN"
	1    1800 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C9
U 1 1 5F981EC3
P 1400 1200
F 0 "C9" H 1492 1246 50  0000 L CNN
F 1 "0.1uF" H 1492 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1400 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1400 1200 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1400 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1400 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1400 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1400 1200 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1400 1200 50  0001 C CNN "Supplier PN"
	1    1400 1200
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0109
U 1 1 5F98252A
P 1000 1350
F 0 "#PWR0109" H 1000 1100 50  0001 C CNN
F 1 "GND" H 1005 1177 50  0000 C CNN
F 2 "" H 1000 1350 50  0001 C CNN
F 3 "" H 1000 1350 50  0001 C CNN
	1    1000 1350
	1    0    0    -1  
$EndComp
Wire Wire Line
	1400 1300 1400 1350
Wire Wire Line
	1400 1350 1800 1350
Wire Wire Line
	3000 1350 3000 1300
Wire Wire Line
	2600 1300 2600 1350
Connection ~ 2600 1350
Wire Wire Line
	2600 1350 3000 1350
Wire Wire Line
	2200 1300 2200 1350
Connection ~ 2200 1350
Wire Wire Line
	2200 1350 2600 1350
Wire Wire Line
	1800 1300 1800 1350
Connection ~ 1800 1350
Wire Wire Line
	1800 1350 2200 1350
$Comp
L power:+3.3V #PWR0110
U 1 1 5F988BE8
P 1000 1050
F 0 "#PWR0110" H 1000 900 50  0001 C CNN
F 1 "+3.3V" H 1015 1223 50  0000 C CNN
F 2 "" H 1000 1050 50  0001 C CNN
F 3 "" H 1000 1050 50  0001 C CNN
	1    1000 1050
	1    0    0    -1  
$EndComp
Wire Wire Line
	1400 1100 1400 1050
Wire Wire Line
	1400 1050 1800 1050
Wire Wire Line
	3800 1050 3800 1550
Wire Wire Line
	1800 1100 1800 1050
Connection ~ 1800 1050
Wire Wire Line
	1800 1050 2200 1050
Wire Wire Line
	2200 1100 2200 1050
Connection ~ 2200 1050
Wire Wire Line
	2200 1050 2600 1050
Wire Wire Line
	2600 1100 2600 1050
Connection ~ 2600 1050
Wire Wire Line
	2600 1050 3000 1050
Wire Wire Line
	3000 1100 3000 1050
Connection ~ 3000 1050
Wire Wire Line
	3700 1550 3700 1050
Connection ~ 3700 1050
Wire Wire Line
	3700 1050 3800 1050
Wire Wire Line
	3600 1550 3600 1050
Wire Wire Line
	3000 1050 3400 1050
Connection ~ 3600 1050
Wire Wire Line
	3600 1050 3700 1050
Wire Wire Line
	3500 1550 3500 1050
Connection ~ 3500 1050
Wire Wire Line
	3500 1050 3600 1050
Wire Wire Line
	3400 1550 3400 1050
Connection ~ 3400 1050
Wire Wire Line
	3400 1050 3500 1050
$Comp
L Device:C_Small C8
U 1 1 5F9A4479
P 1000 1200
F 0 "C8" H 1092 1246 50  0000 L CNN
F 1 "10uF" H 1092 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 1000 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1000 1200 50  0001 C CNN
F 4 "CAP CER 10UF 25V X5R 0805" H 1000 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1000 1200 50  0001 C CNN "Manufacturer"
F 6 "CL21A106KAYNNNG" H 1000 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1000 1200 50  0001 C CNN "Supplier"
F 8 "1276-6454-1-ND" H 1000 1200 50  0001 C CNN "Supplier PN"
	1    1000 1200
	1    0    0    -1  
$EndComp
Wire Wire Line
	1000 1100 1000 1050
Wire Wire Line
	1400 1050 1000 1050
Connection ~ 1400 1050
Connection ~ 1000 1050
Wire Wire Line
	1400 1350 1000 1350
Connection ~ 1400 1350
Wire Wire Line
	1000 1300 1000 1350
Connection ~ 1000 1350
Text Notes 1400 950  0    50   ~ 0
0.1uF caps are for each power pin
$Comp
L Device:C_Small C20
U 1 1 5F9AEFEC
P 2650 2300
F 0 "C20" H 2742 2346 50  0000 L CNN
F 1 "4.7uF" H 2742 2255 50  0000 L CNN
F 2 "Capacitor_SMD:C_1206_3216Metric" H 2650 2300 50  0001 C CNN
F 3 "https://www.yageo.com/upload/media/product/productsearch/datasheet/mlcc/UPY-GPHC_X7R_6.3V-to-50V_18.pdf" H 2650 2300 50  0001 C CNN
F 4 "CAP CER 4.7UF 25V X7R 1206" H 2650 2300 50  0001 C CNN "Description"
F 5 "Yageo" H 2650 2300 50  0001 C CNN "Manufacturer"
F 6 "CC1206KKX7R8BB475" H 2650 2300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2650 2300 50  0001 C CNN "Supplier"
F 8 "311-1961-1-ND" H 2650 2300 50  0001 C CNN "Supplier PN"
	1    2650 2300
	1    0    0    -1  
$EndComp
Text Notes 2050 2700 0    50   ~ 0
VCAP needs\n<1ohm ESR
$Comp
L power:GND #PWR0111
U 1 1 5F9AF7F7
P 2650 2400
F 0 "#PWR0111" H 2650 2150 50  0001 C CNN
F 1 "GND" H 2655 2227 50  0000 C CNN
F 2 "" H 2650 2400 50  0001 C CNN
F 3 "" H 2650 2400 50  0001 C CNN
	1    2650 2400
	1    0    0    -1  
$EndComp
Wire Wire Line
	2900 2150 2650 2150
Wire Wire Line
	2650 2150 2650 2200
$Comp
L Device:R_Small R1
U 1 1 5F9B3310
P 2350 2150
F 0 "R1" H 2409 2196 50  0000 L CNN
F 1 "10K" H 2409 2105 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 2350 2150 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2350 2150 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2350 2150 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2350 2150 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2350 2150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2350 2150 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2350 2150 50  0001 C CNN "Supplier PN"
	1    2350 2150
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0112
U 1 1 5F9B41EF
P 2350 2250
F 0 "#PWR0112" H 2350 2000 50  0001 C CNN
F 1 "GND" H 2355 2077 50  0000 C CNN
F 2 "" H 2350 2250 50  0001 C CNN
F 3 "" H 2350 2250 50  0001 C CNN
	1    2350 2250
	1    0    0    -1  
$EndComp
Wire Wire Line
	2900 1950 2350 1950
Wire Wire Line
	2350 1950 2350 2050
$Comp
L power:GND #PWR0113
U 1 1 5F9BB459
P 3800 5150
F 0 "#PWR0113" H 3800 4900 50  0001 C CNN
F 1 "GND" H 3805 4977 50  0000 C CNN
F 2 "" H 3800 5150 50  0001 C CNN
F 3 "" H 3800 5150 50  0001 C CNN
	1    3800 5150
	1    0    0    -1  
$EndComp
Wire Wire Line
	3400 5050 3400 5150
Wire Wire Line
	3400 5150 3500 5150
Wire Wire Line
	3500 5050 3500 5150
Connection ~ 3500 5150
Wire Wire Line
	3500 5150 3600 5150
Wire Wire Line
	3600 5050 3600 5150
Connection ~ 3600 5150
Wire Wire Line
	3600 5150 3700 5150
Wire Wire Line
	3700 5050 3700 5150
Connection ~ 3700 5150
Wire Wire Line
	3700 5150 3800 5150
Wire Wire Line
	3800 5050 3800 5150
Connection ~ 3800 5150
$Comp
L Device:C_Small C19
U 1 1 5F9CAEBD
P 1950 1950
F 0 "C19" H 2042 1996 50  0000 L CNN
F 1 "0.1uF" H 2042 1905 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1950 1950 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1950 1950 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1950 1950 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1950 1950 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1950 1950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1950 1950 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1950 1950 50  0001 C CNN "Supplier PN"
	1    1950 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2900 1750 1950 1750
Wire Wire Line
	1950 1750 1950 1850
$Comp
L power:GND #PWR0114
U 1 1 5F9CF1A9
P 1950 2050
F 0 "#PWR0114" H 1950 1800 50  0001 C CNN
F 1 "GND" H 1955 1877 50  0000 C CNN
F 2 "" H 1950 2050 50  0001 C CNN
F 3 "" H 1950 2050 50  0001 C CNN
	1    1950 2050
	1    0    0    -1  
$EndComp
Text Label 1650 1750 0    50   ~ 0
RESET
Wire Wire Line
	1950 1750 1650 1750
Connection ~ 1950 1750
Text Label 2300 6250 0    50   ~ 0
RESET
Text Label 4400 1950 0    50   ~ 0
BUZZ_DIN
Wire Wire Line
	4300 1950 5000 1950
Text Label 4400 2150 0    50   ~ 0
DISP_~CS
Text Label 4400 2250 0    50   ~ 0
DISP_SPI_SCK
Text Label 4400 2350 0    50   ~ 0
DISP_D~C
Text Label 4400 2450 0    50   ~ 0
DISP_SPI_MOSI
Wire Wire Line
	4300 2450 5000 2450
Wire Wire Line
	4300 2350 5000 2350
Wire Wire Line
	4300 2250 5000 2250
Wire Wire Line
	4300 2150 5000 2150
Text Label 2250 3750 0    50   ~ 0
DISP_~RES
Text Label 2250 3450 0    50   ~ 0
USB_DRIVE_VBUS
Text Label 2250 3550 0    50   ~ 0
USB_VBUS_OC
Text Label 4400 3650 0    50   ~ 0
SENSOR_~INT
Wire Wire Line
	4300 3650 5000 3650
Text Label 4400 4450 0    50   ~ 0
I2C2_SCL
Wire Wire Line
	4300 4450 5000 4450
Text Label 2250 4250 0    50   ~ 0
RELAY_ENLG
Text Label 4400 2550 0    50   ~ 0
ENC_CH1
Text Label 4400 2650 0    50   ~ 0
ENC_CH2
Wire Wire Line
	4300 2550 5000 2550
Wire Wire Line
	4300 2650 5000 2650
Text Label 4400 2750 0    50   ~ 0
USART1_RX
Wire Wire Line
	4300 2750 5000 2750
Text Label 4400 2850 0    50   ~ 0
USB_OTG_FS_DM
Text Label 4400 2950 0    50   ~ 0
USB_OTG_FS_DP
Wire Wire Line
	4300 2850 5000 2850
Wire Wire Line
	4300 2950 5000 2950
Text Label 4400 3050 0    50   ~ 0
JTMS_SWDIO
Text Label 4400 3150 0    50   ~ 0
JCLK_SWCLK
Text Label 4400 3250 0    50   ~ 0
JTDI
Wire Wire Line
	4300 3050 5000 3050
Wire Wire Line
	4300 3150 5000 3150
Wire Wire Line
	4300 3250 5000 3250
Text Label 4400 3750 0    50   ~ 0
JTDO_SWO
Wire Wire Line
	4300 3750 5000 3750
Text Label 4400 3950 0    50   ~ 0
KEY_~INT
Text Label 4400 4050 0    50   ~ 0
USART1_TX
Text Label 4400 4250 0    50   ~ 0
I2C1_SCL
Text Label 4400 4350 0    50   ~ 0
I2C2_SDA
Wire Wire Line
	4300 4350 5000 4350
Wire Wire Line
	4300 4050 5000 4050
Wire Wire Line
	4300 3950 5000 3950
Wire Wire Line
	2200 6250 2850 6250
NoConn ~ 2200 6950
$Comp
L power:GND #PWR014
U 1 1 5FB2CF53
P 1600 7500
F 0 "#PWR014" H 1600 7250 50  0001 C CNN
F 1 "GND" H 1605 7327 50  0000 C CNN
F 2 "" H 1600 7500 50  0001 C CNN
F 3 "" H 1600 7500 50  0001 C CNN
	1    1600 7500
	1    0    0    -1  
$EndComp
Wire Wire Line
	1700 7450 1600 7450
Wire Wire Line
	1600 7450 1600 7500
Connection ~ 1600 7450
$Comp
L Project:ESDALC6V1W5 D11
U 1 1 5F97AB57
P 3150 6600
F 0 "D11" H 3150 6992 50  0000 C CNN
F 1 "ESDALC6V1W5" H 3150 6901 50  0000 C CNN
F 2 "lib_fp:SOT323-5L" H 3150 6225 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/32/30/02/e6/ac/0f/46/c2/CD00002946.pdf/files/CD00002946.pdf/jcr:content/translations/en.CD00002946.pdf" H 3150 6650 50  0001 C CNN
F 4 "TVS DIODE 3V SOT323-5" H 3150 6600 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3150 6600 50  0001 C CNN "Manufacturer"
F 6 "ESDALC6V1W5" H 3150 6600 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 6600 50  0001 C CNN "Supplier"
F 8 "497-7231-1-ND" H 3150 6600 50  0001 C CNN "Supplier PN"
	1    3150 6600
	1    0    0    -1  
$EndComp
$Comp
L Project:ESDALC6V1W5 D12
U 1 1 5F97DF98
P 3150 7300
F 0 "D12" H 3150 7692 50  0000 C CNN
F 1 "ESDALC6V1W5" H 3150 7601 50  0000 C CNN
F 2 "lib_fp:SOT323-5L" H 3150 6925 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/32/30/02/e6/ac/0f/46/c2/CD00002946.pdf/files/CD00002946.pdf/jcr:content/translations/en.CD00002946.pdf" H 3150 7350 50  0001 C CNN
F 4 "TVS DIODE 3V SOT323-5" H 3150 7300 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3150 7300 50  0001 C CNN "Manufacturer"
F 6 "ESDALC6V1W5" H 3150 7300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 7300 50  0001 C CNN "Supplier"
F 8 "497-7231-1-ND" H 3150 7300 50  0001 C CNN "Supplier PN"
	1    3150 7300
	1    0    0    -1  
$EndComp
Wire Wire Line
	2200 6450 2950 6450
Wire Wire Line
	2200 6550 2950 6550
Wire Wire Line
	2200 6650 2950 6650
Wire Wire Line
	2200 6750 2950 6750
Wire Wire Line
	2200 7150 2800 7150
Wire Wire Line
	2200 7250 2750 7250
NoConn ~ 2950 7450
$Comp
L power:GND #PWR012
U 1 1 5F9D5CF7
P 3400 7350
F 0 "#PWR012" H 3400 7100 50  0001 C CNN
F 1 "GND" H 3405 7177 50  0000 C CNN
F 2 "" H 3400 7350 50  0001 C CNN
F 3 "" H 3400 7350 50  0001 C CNN
	1    3400 7350
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR011
U 1 1 5F9D628B
P 3400 6650
F 0 "#PWR011" H 3400 6400 50  0001 C CNN
F 1 "GND" H 3405 6477 50  0000 C CNN
F 2 "" H 3400 6650 50  0001 C CNN
F 3 "" H 3400 6650 50  0001 C CNN
	1    3400 6650
	1    0    0    -1  
$EndComp
Wire Wire Line
	3350 6600 3400 6600
Wire Wire Line
	3400 6600 3400 6650
Wire Wire Line
	3350 7300 3400 7300
Wire Wire Line
	3400 7300 3400 7350
Text Label 2300 6450 0    50   ~ 0
JCLK_SWCLK
Text Label 2300 6550 0    50   ~ 0
JTMS_SWDIO
Text Label 2300 6650 0    50   ~ 0
JTDO_SWO
Text Label 2300 6750 0    50   ~ 0
JTDI
Text Label 2300 7150 0    50   ~ 0
USART1_RX
Text Label 2300 7250 0    50   ~ 0
USART1_TX
Wire Wire Line
	2750 7250 2750 7350
Wire Wire Line
	2750 7350 2950 7350
Wire Wire Line
	2800 7150 2800 7250
Wire Wire Line
	2800 7250 2950 7250
Wire Wire Line
	2850 6250 2850 7150
Wire Wire Line
	2850 7150 2950 7150
Text Notes 2250 7750 0    50   ~ 0
Can swap connections on ESD\nprotection to optimize routing
NoConn ~ 1700 6050
Text Label 5900 3550 0    50   ~ 0
I2C2_SCL
Text Label 5900 3450 0    50   ~ 0
I2C2_SDA
Text Label 5900 3800 0    50   ~ 0
SENSOR_~INT
Text Label 5900 4350 0    50   ~ 0
USB_OTG_FS_DM
Text Label 5900 4250 0    50   ~ 0
USB_OTG_FS_DP
Text Label 5900 4050 0    50   ~ 0
USB_DRIVE_VBUS
Text Label 5900 4150 0    50   ~ 0
USB_VBUS_OC
Text Label 5900 2450 0    50   ~ 0
SENSOR_BTN
Text Label 5900 2550 0    50   ~ 0
FOOTSWITCH
Text Label 5900 3700 0    50   ~ 0
SENSOR_BTN
Text Label 5900 3900 0    50   ~ 0
FOOTSWITCH
Wire Wire Line
	6550 2450 5900 2450
Wire Wire Line
	5900 2550 6550 2550
Text Label 5900 1850 0    50   ~ 0
I2C1_SDA
Text Label 5900 1950 0    50   ~ 0
I2C1_SCL
Wire Wire Line
	5900 1950 6550 1950
Wire Wire Line
	5900 1850 6550 1850
Text Label 5900 2050 0    50   ~ 0
KEY_~INT
Wire Wire Line
	5900 2050 6550 2050
Text Notes 4800 4650 0    50   ~ 0
I2C2 has pull-ups as part\nof main-board-front
Text Label 5900 950  0    50   ~ 0
RELAY_SFLT
Text Label 5900 850  0    50   ~ 0
RELAY_ENLG
Wire Wire Line
	6550 850  5900 850 
Wire Wire Line
	6550 950  5900 950 
Text Label 2250 4150 0    50   ~ 0
RELAY_SFLT
Wire Wire Line
	2900 3450 2250 3450
Wire Wire Line
	2900 3550 2250 3550
Wire Wire Line
	2900 3750 2250 3750
Wire Wire Line
	2900 4150 2250 4150
Wire Wire Line
	2900 4250 2250 4250
$Comp
L power:+3.3V #PWR06
U 1 1 5FA693EA
P 6000 5350
F 0 "#PWR06" H 6000 5200 50  0001 C CNN
F 1 "+3.3V" H 6015 5523 50  0000 C CNN
F 2 "" H 6000 5350 50  0001 C CNN
F 3 "" H 6000 5350 50  0001 C CNN
	1    6000 5350
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C23
U 1 1 5FA699B6
P 6150 5450
F 0 "C23" V 6300 5300 50  0000 L CNN
F 1 "0.1uF" V 6200 5200 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 6150 5450 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 6150 5450 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 6150 5450 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 6150 5450 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 6150 5450 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 6150 5450 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 6150 5450 50  0001 C CNN "Supplier PN"
	1    6150 5450
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR07
U 1 1 5FA6B086
P 6250 5450
F 0 "#PWR07" H 6250 5200 50  0001 C CNN
F 1 "GND" H 6255 5277 50  0000 C CNN
F 2 "" H 6250 5450 50  0001 C CNN
F 3 "" H 6250 5450 50  0001 C CNN
	1    6250 5450
	1    0    0    -1  
$EndComp
Wire Wire Line
	6000 5600 6000 5450
Wire Wire Line
	6050 5450 6000 5450
Connection ~ 6000 5450
Wire Wire Line
	6000 5450 6000 5350
$Comp
L power:GND #PWR010
U 1 1 5FA90C2E
P 6000 6200
F 0 "#PWR010" H 6000 5950 50  0001 C CNN
F 1 "GND" H 6005 6027 50  0000 C CNN
F 2 "" H 6000 6200 50  0001 C CNN
F 3 "" H 6000 6200 50  0001 C CNN
	1    6000 6200
	1    0    0    -1  
$EndComp
Text Label 4400 4150 0    50   ~ 0
I2C1_SDA
Text Label 6800 5800 2    50   ~ 0
I2C1_SDA
Text Label 6800 5900 2    50   ~ 0
I2C1_SCL
$Comp
L Device:R_Small R2
U 1 1 5FA91E20
P 5150 4000
F 0 "R2" H 5209 4046 50  0000 L CNN
F 1 "2K" H 5209 3955 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 5150 4000 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 5150 4000 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 5150 4000 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 5150 4000 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 5150 4000 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5150 4000 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 5150 4000 50  0001 C CNN "Supplier PN"
	1    5150 4000
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R3
U 1 1 5FA9230A
P 5400 4000
F 0 "R3" H 5459 4046 50  0000 L CNN
F 1 "2K" H 5459 3955 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 5400 4000 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 5400 4000 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 5400 4000 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 5400 4000 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 5400 4000 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5400 4000 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 5400 4000 50  0001 C CNN "Supplier PN"
	1    5400 4000
	1    0    0    -1  
$EndComp
Wire Wire Line
	5150 4150 5150 4100
Wire Wire Line
	4300 4150 5150 4150
Wire Wire Line
	5400 4250 5400 4100
Wire Wire Line
	4300 4250 5400 4250
$Comp
L power:+3.3V #PWR05
U 1 1 5FAB8BA0
P 5400 3800
F 0 "#PWR05" H 5400 3650 50  0001 C CNN
F 1 "+3.3V" H 5415 3973 50  0000 C CNN
F 2 "" H 5400 3800 50  0001 C CNN
F 3 "" H 5400 3800 50  0001 C CNN
	1    5400 3800
	1    0    0    -1  
$EndComp
Wire Wire Line
	5400 3900 5400 3850
Wire Wire Line
	5150 3900 5150 3850
Wire Wire Line
	5150 3850 5400 3850
Connection ~ 5400 3850
Wire Wire Line
	5400 3850 5400 3800
Wire Wire Line
	6400 5800 6800 5800
Wire Wire Line
	6400 5900 6800 5900
Wire Wire Line
	6400 6000 6650 6000
$Comp
L power:GND #PWR08
U 1 1 5FB2CC3B
P 5550 6050
F 0 "#PWR08" H 5550 5800 50  0001 C CNN
F 1 "GND" H 5555 5877 50  0000 C CNN
F 2 "" H 5550 6050 50  0001 C CNN
F 3 "" H 5550 6050 50  0001 C CNN
	1    5550 6050
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR09
U 1 1 5FB2D122
P 6650 6050
F 0 "#PWR09" H 6650 5800 50  0001 C CNN
F 1 "GND" H 6655 5877 50  0000 C CNN
F 2 "" H 6650 6050 50  0001 C CNN
F 3 "" H 6650 6050 50  0001 C CNN
	1    6650 6050
	1    0    0    -1  
$EndComp
Wire Wire Line
	6650 6000 6650 6050
Wire Wire Line
	5600 5900 5550 5900
Wire Wire Line
	5550 5900 5550 6000
Wire Wire Line
	5600 6000 5550 6000
Connection ~ 5550 6000
Wire Wire Line
	5550 6000 5550 6050
$Comp
L Project:PAM8904EJP U5
U 1 1 5FB60707
P 8900 1550
F 0 "U5" H 8650 2050 50  0000 C CNN
F 1 "PAM8904EJP" H 9200 1000 50  0000 C CNN
F 2 "lib_fp:U-QFN3030-12_Type-A" H 8900 550 50  0001 C CNN
F 3 "https://www.diodes.com/assets/Datasheets/PAM8904E.pdf" H 8900 1550 50  0001 C CNN
F 4 "AUDIO HIGH VOLT U-QFN3030-16" H 8900 1550 50  0001 C CNN "Description"
F 5 "Diodes Incorporated" H 8900 1550 50  0001 C CNN "Manufacturer"
F 6 "PAM8904EJER" H 8900 1550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 8900 1550 50  0001 C CNN "Supplier"
F 8 "31-PAM8904EJERCT-ND" H 8900 1550 50  0001 C CNN "Supplier PN"
	1    8900 1550
	1    0    0    -1  
$EndComp
$Comp
L Device:Speaker_Crystal LS?
U 1 1 5FB6070D
P 10000 1850
AR Path="/5FB5745F/5FB6070D" Ref="LS?"  Part="1" 
AR Path="/5FB6070D" Ref="LS1"  Part="1" 
F 0 "LS1" H 10175 1846 50  0000 L CNN
F 1 "Buzzer" H 10175 1755 50  0000 L CNN
F 2 "lib_fp:PKM22EPPH4001-B0" H 9965 1800 50  0001 C CNN
F 3 "https://www.murata.com/~/media/webrenewal/support/library/catalog/products/sound/p37e.ashx?la=en-us" H 9965 1800 50  0001 C CNN
F 4 "AUDIO PIEZO TRANSDUCER 30V TH" H 10000 1850 50  0001 C CNN "Description"
F 5 "Murata Electronics" H 10000 1850 50  0001 C CNN "Manufacturer"
F 6 "PKM22EPPH4001-B0" H 10000 1850 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 10000 1850 50  0001 C CNN "Supplier"
F 8 "490-4692-ND" H 10000 1850 50  0001 C CNN "Supplier PN"
	1    10000 1850
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C7
U 1 1 5FB6071E
P 9100 900
F 0 "C7" V 8871 900 50  0000 C CNN
F 1 "2.2uF" V 8962 900 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 9100 900 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9100 900 50  0001 C CNN
F 4 "CAP CER 2.2UF 25V X5R 0805" H 9100 900 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9100 900 50  0001 C CNN "Manufacturer"
F 6 "CL21A225KAFNNNG" H 9100 900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9100 900 50  0001 C CNN "Supplier"
F 8 "1276-6458-1-ND" H 9100 900 50  0001 C CNN "Supplier PN"
	1    9100 900 
	0    1    1    0   
$EndComp
$Comp
L Device:C_Small C17
U 1 1 5FB60729
P 9550 1400
F 0 "C17" H 9642 1446 50  0000 L CNN
F 1 "1uF" H 9642 1355 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 9550 1400 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9550 1400 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 9550 1400 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9550 1400 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 9550 1400 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9550 1400 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 9550 1400 50  0001 C CNN "Supplier PN"
	1    9550 1400
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C18
U 1 1 5FB60734
P 9550 1650
F 0 "C18" H 9642 1696 50  0000 L CNN
F 1 "1uF" H 9642 1605 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 9550 1650 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9550 1650 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 9550 1650 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9550 1650 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 9550 1650 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9550 1650 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 9550 1650 50  0001 C CNN "Supplier PN"
	1    9550 1650
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C16
U 1 1 5FB6073F
P 9900 1300
F 0 "C16" H 9992 1346 50  0000 L CNN
F 1 "2.2uF" H 9992 1255 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 9900 1300 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9900 1300 50  0001 C CNN
F 4 "CAP CER 2.2UF 25V X5R 0805" H 9900 1300 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9900 1300 50  0001 C CNN "Manufacturer"
F 6 "CL21A225KAFNNNG" H 9900 1300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9900 1300 50  0001 C CNN "Supplier"
F 8 "1276-6458-1-ND" H 9900 1300 50  0001 C CNN "Supplier PN"
	1    9900 1300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR04
U 1 1 5FB60745
P 8900 2150
F 0 "#PWR04" H 8900 1900 50  0001 C CNN
F 1 "GND" H 8905 1977 50  0000 C CNN
F 2 "" H 8900 2150 50  0001 C CNN
F 3 "" H 8900 2150 50  0001 C CNN
	1    8900 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	8900 1000 8900 900 
Wire Wire Line
	9000 900  8900 900 
Connection ~ 8900 900 
Wire Wire Line
	8900 900  8900 800 
$Comp
L power:GND #PWR02
U 1 1 5FB6077A
P 9300 900
F 0 "#PWR02" H 9300 650 50  0001 C CNN
F 1 "GND" H 9305 727 50  0000 C CNN
F 2 "" H 9300 900 50  0001 C CNN
F 3 "" H 9300 900 50  0001 C CNN
	1    9300 900 
	1    0    0    -1  
$EndComp
Wire Wire Line
	9200 900  9300 900 
Wire Wire Line
	9300 1350 9400 1350
Wire Wire Line
	9400 1350 9400 1300
Wire Wire Line
	9400 1300 9550 1300
Wire Wire Line
	9300 1450 9400 1450
Wire Wire Line
	9400 1450 9400 1500
Wire Wire Line
	9400 1500 9550 1500
Wire Wire Line
	9300 1600 9400 1600
Wire Wire Line
	9400 1600 9400 1550
Wire Wire Line
	9400 1550 9550 1550
Wire Wire Line
	9300 1700 9400 1700
Wire Wire Line
	9400 1700 9400 1750
Wire Wire Line
	9400 1750 9550 1750
Wire Wire Line
	9300 1200 9900 1200
$Comp
L power:GND #PWR03
U 1 1 5FB6078E
P 9900 1400
F 0 "#PWR03" H 9900 1150 50  0001 C CNN
F 1 "GND" H 9905 1227 50  0000 C CNN
F 2 "" H 9900 1400 50  0001 C CNN
F 3 "" H 9900 1400 50  0001 C CNN
	1    9900 1400
	1    0    0    -1  
$EndComp
Wire Wire Line
	9300 1850 9800 1850
Wire Wire Line
	9800 1950 9300 1950
$Comp
L power:+3.3V #PWR01
U 1 1 5FBD6808
P 8900 800
F 0 "#PWR01" H 8900 650 50  0001 C CNN
F 1 "+3.3V" H 8915 973 50  0000 C CNN
F 2 "" H 8900 800 50  0001 C CNN
F 3 "" H 8900 800 50  0001 C CNN
	1    8900 800 
	1    0    0    -1  
$EndComp
Text Label 2250 4050 0    50   ~ 0
LED_CLK
Text Label 2250 3950 0    50   ~ 0
LED_~OE
Text Label 4400 4850 0    50   ~ 0
LED_SDI
Text Label 4400 4750 0    50   ~ 0
LED_LE
Wire Wire Line
	2250 4050 2900 4050
Wire Wire Line
	2250 3950 2900 3950
Wire Wire Line
	4300 4750 5000 4750
Wire Wire Line
	4300 4850 5000 4850
Text Label 5900 3000 0    50   ~ 0
LED_~OE
Text Label 5900 2800 0    50   ~ 0
LED_CLK
Text Label 5900 2900 0    50   ~ 0
LED_LE
Text Label 5900 2700 0    50   ~ 0
LED_SDI
Wire Wire Line
	6550 2700 5900 2700
Wire Wire Line
	6550 2800 5900 2800
Wire Wire Line
	6550 2900 5900 2900
Wire Wire Line
	6550 3000 5900 3000
NoConn ~ 4300 2050
NoConn ~ 4300 3450
NoConn ~ 4300 3550
NoConn ~ 4300 3850
NoConn ~ 4300 4550
NoConn ~ 4300 4650
NoConn ~ 2900 4850
NoConn ~ 2900 4750
NoConn ~ 2900 4650
NoConn ~ 2900 4550
NoConn ~ 2900 4450
NoConn ~ 2900 4350
NoConn ~ 2900 3850
NoConn ~ 2900 3650
NoConn ~ 2900 3350
NoConn ~ 2900 3150
$Comp
L power:PWR_FLAG #FLG0108
U 1 1 5FB7BB20
P 3900 1050
F 0 "#FLG0108" H 3900 1125 50  0001 C CNN
F 1 "PWR_FLAG" H 3900 1223 50  0001 C CNN
F 2 "" H 3900 1050 50  0001 C CNN
F 3 "~" H 3900 1050 50  0001 C CNN
	1    3900 1050
	1    0    0    -1  
$EndComp
Connection ~ 3900 1050
$Comp
L power:PWR_FLAG #FLG0109
U 1 1 5FB7C012
P 2650 2150
F 0 "#FLG0109" H 2650 2225 50  0001 C CNN
F 1 "PWR_FLAG" H 2650 2323 50  0001 C CNN
F 2 "" H 2650 2150 50  0001 C CNN
F 3 "~" H 2650 2150 50  0001 C CNN
	1    2650 2150
	1    0    0    -1  
$EndComp
Connection ~ 2650 2150
NoConn ~ 1450 7450
Wire Wire Line
	5900 4350 6550 4350
Wire Wire Line
	5900 4250 6550 4250
Wire Wire Line
	5900 4150 6550 4150
Wire Wire Line
	6550 4050 5900 4050
Wire Wire Line
	5900 3900 6550 3900
Wire Wire Line
	5900 3800 6550 3800
Wire Wire Line
	5900 3700 6550 3700
Wire Wire Line
	5900 3550 6550 3550
Wire Wire Line
	6550 3450 5900 3450
$Sheet
S 6550 3350 1400 1100
U 5FA19AFF
F0 "Front External Connections" 50
F1 "main-board-front.sch" 50
F2 "EXT_I2C_SDA" B L 6550 3450 50 
F3 "EXT_I2C_SCL" B L 6550 3550 50 
F4 "SENSOR_BTN" O L 6550 3700 50 
F5 "SENSOR_INT" O L 6550 3800 50 
F6 "FOOTSWITCH" O L 6550 3900 50 
F7 "USB_DRIVE_VBUS" I L 6550 4050 50 
F8 "USB_VBUS_OC" O L 6550 4150 50 
F9 "USB_DP" B L 6550 4250 50 
F10 "USB_DM" B L 6550 4350 50 
$EndSheet
Text Label 5900 2200 0    50   ~ 0
ENC_CH1
Text Label 5900 2300 0    50   ~ 0
ENC_CH2
Wire Wire Line
	5900 2200 6550 2200
Wire Wire Line
	5900 2300 6550 2300
Text Label 4400 1850 0    50   ~ 0
BUZZ_EN2
Text Label 4400 1750 0    50   ~ 0
BUZZ_EN1
Wire Wire Line
	4300 1850 5000 1850
Wire Wire Line
	4300 1750 5000 1750
Text Label 8100 1600 0    50   ~ 0
BUZZ_DIN
Text Label 8100 1350 0    50   ~ 0
BUZZ_EN1
Text Label 8100 1450 0    50   ~ 0
BUZZ_EN2
Wire Wire Line
	8500 1350 8100 1350
Wire Wire Line
	8500 1450 8100 1450
Wire Wire Line
	8500 1600 8100 1600
$EndSCHEMATC
