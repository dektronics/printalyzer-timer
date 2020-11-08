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
P 3350 3200
F 0 "U4" H 2800 4850 50  0000 C CNN
F 1 "STM32F411RETx" H 3900 1450 50  0000 C CNN
F 2 "Package_QFP:LQFP-64_10x10mm_P0.5mm" H 2750 1500 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00115249.pdf" H 3350 3200 50  0001 C CNN
F 4 "IC MCU 32BIT 512KB FLASH 64LQFP" H 3350 3200 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3350 3200 50  0001 C CNN "Manufacturer"
F 6 "STM32F411RET6" H 3350 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3350 3200 50  0001 C CNN "Supplier"
F 8 "497-14909-ND" H 3350 3200 50  0001 C CNN "Supplier PN"
	1    3350 3200
	1    0    0    -1  
$EndComp
$Sheet
S 6500 750  1400 750 
U 5F8B674D
F0 "Power Supply and Control" 50
F1 "main-board-power.sch" 50
F2 "RELAY_ENLARGER" I L 6500 850 50 
F3 "RELAY_SAFELIGHT" I L 6500 950 50 
$EndSheet
$Sheet
S 6500 1750 1400 1350
U 5F8B69B5
F0 "User Input" 50
F1 "main-board-input.sch" 50
F2 "INT_I2C_SDA" B L 6500 1850 50 
F3 "INT_I2C_SCL" B L 6500 1950 50 
F4 "SENSOR_BTN" I L 6500 2450 50 
F5 "FOOTSWITCH" I L 6500 2550 50 
F6 "LED_SDI" I L 6500 2700 50 
F7 "LED_CLK" I L 6500 2800 50 
F8 "LED_LE" I L 6500 2900 50 
F9 "LED_~OE" I L 6500 3000 50 
F10 "KEY_~INT" O L 6500 2050 50 
F11 "ENC_A" O L 6500 2200 50 
F12 "ENC_B" O L 6500 2300 50 
$EndSheet
$Comp
L power:GND #PWR0103
U 1 1 5F8B9F00
P 9550 4950
F 0 "#PWR0103" H 9550 4700 50  0001 C CNN
F 1 "GND" H 9555 4777 50  0000 C CNN
F 2 "" H 9550 4950 50  0001 C CNN
F 3 "" H 9550 4950 50  0001 C CNN
	1    9550 4950
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR0104
U 1 1 5F8BA7DD
P 9500 2900
F 0 "#PWR0104" H 9500 2750 50  0001 C CNN
F 1 "+3.3V" H 9515 3073 50  0000 C CNN
F 2 "" H 9500 2900 50  0001 C CNN
F 3 "" H 9500 2900 50  0001 C CNN
	1    9500 2900
	1    0    0    -1  
$EndComp
Wire Wire Line
	9650 3050 9500 3050
Wire Wire Line
	9500 3050 9500 2900
NoConn ~ 9650 3150
NoConn ~ 9650 3750
NoConn ~ 9650 4350
NoConn ~ 9650 4650
Wire Wire Line
	9650 2950 9550 2950
Wire Wire Line
	9550 2950 9550 3350
Wire Wire Line
	9650 3350 9550 3350
Connection ~ 9550 3350
Wire Wire Line
	9550 3350 9550 3450
Wire Wire Line
	9650 3450 9550 3450
Connection ~ 9550 3450
Wire Wire Line
	9650 3850 9550 3850
Connection ~ 9550 3850
Wire Wire Line
	9550 3850 9550 3950
Wire Wire Line
	9650 3950 9550 3950
Connection ~ 9550 3950
Wire Wire Line
	9550 3950 9550 4050
Wire Wire Line
	9650 4050 9550 4050
Connection ~ 9550 4050
Wire Wire Line
	9550 4050 9550 4150
Wire Wire Line
	9650 4150 9550 4150
Connection ~ 9550 4150
Wire Wire Line
	9550 4150 9550 4250
Wire Wire Line
	9650 4250 9550 4250
Connection ~ 9550 4250
Wire Wire Line
	9550 4250 9550 4750
Wire Wire Line
	9650 4750 9550 4750
Connection ~ 9550 4750
Wire Wire Line
	9550 4750 9550 4850
Wire Wire Line
	9650 4850 9550 4850
Connection ~ 9550 4850
Wire Wire Line
	9550 4850 9550 4950
Wire Wire Line
	9650 3250 8950 3250
Wire Wire Line
	9650 3550 8950 3550
Wire Wire Line
	9650 4450 8950 4450
Wire Wire Line
	9650 4550 8950 4550
Text Label 8950 3250 0    50   ~ 0
DISP_D~C
Text Label 8950 3550 0    50   ~ 0
DISP_SPI_SCK
Text Label 8950 3650 0    50   ~ 0
DISP_SPI_MOSI
Text Label 8950 4450 0    50   ~ 0
DISP_~RES
Text Label 8950 4550 0    50   ~ 0
DISP_~CS
$Comp
L Project:Conn_ST_STDC14 J2
U 1 1 5FA6AC8D
P 1350 6400
F 0 "J2" H 907 6446 50  0000 R CNN
F 1 "STDC14" H 907 6355 50  0000 R CNN
F 2 "lib_fp:Samtec_FTSH-107-01-L-DV-K-A_2x07_P1.27mm" H 1350 5300 50  0001 C CNN
F 3 "https://www.mouser.com/datasheet/2/527/ftsh_smt-1316912.pdf" V 1000 5150 50  0001 C CNN
F 4 "CONN HEADER SMD 14POS 1.27MM" H 1350 6400 50  0001 C CNN "Description"
F 5 "Samtec Inc." H 1350 6400 50  0001 C CNN "Manufacturer"
F 6 "FTSH-107-01-L-DV-K-A" H 1350 6400 50  0001 C CNN "Manufacturer PN"
F 7 "Mouser" H 1350 6400 50  0001 C CNN "Supplier"
F 8 "200-FTSH10701LDVKA" H 1350 6400 50  0001 C CNN "Supplier PN"
	1    1350 6400
	1    0    0    -1  
$EndComp
Wire Wire Line
	9550 3450 9550 3850
Wire Wire Line
	9650 3650 8950 3650
$Comp
L Connector_Generic:Conn_01x20 J3
U 1 1 5F8B8832
P 9850 3850
F 0 "J3" H 9800 4850 50  0000 L CNN
F 1 "NHD OLED Connector" H 9600 2750 50  0000 L CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x20_P2.54mm_Vertical" H 9850 3850 50  0001 C CNN
F 3 "https://drawings-pdf.s3.amazonaws.com/10492.pdf" H 9850 3850 50  0001 C CNN
F 4 "CONN HDR 20POS 0.1 GOLD PCB" H 9850 3850 50  0001 C CNN "Description"
F 5 "Sullins Connector Solutions" H 9850 3850 50  0001 C CNN "Manufacturer"
F 6 "PPPC201LFBN-RC" H 9850 3850 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9850 3850 50  0001 C CNN "Supplier"
F 8 "PPPC201LFBN-RC" H 9850 3850 50  0001 C CNN "Supplier PN"
	1    9850 3850
	1    0    0    -1  
$EndComp
$Comp
L Device:Crystal_GND24 Y1
U 1 1 5F94F8E6
P 1200 3150
F 0 "Y1" H 1250 3350 50  0000 L CNN
F 1 "16MHz" H 1225 2950 50  0000 L CNN
F 2 "lib_fp:Crystal_SMD_Kyocera_CX3225GB" H 1200 3150 50  0001 C CNN
F 3 "http://prdct-search.kyocera.co.jp/crystal-ic/catalog/en/cx3225gb_e.pdf" H 1200 3150 50  0001 C CNN
F 4 "CRYSTAL 16.0000MHZ 8PF SMD" H 1200 3150 50  0001 C CNN "Description"
F 5 "Kyocera International Inc." H 1200 3150 50  0001 C CNN "Manufacturer"
F 6 "CX3225GB16000D0HPQCC" H 1200 3150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1200 3150 50  0001 C CNN "Supplier"
F 8 "1253-1181-1-ND" H 1200 3150 50  0001 C CNN "Supplier PN"
	1    1200 3150
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C21
U 1 1 5F9506A3
P 900 3500
F 0 "C21" H 992 3546 50  0000 L CNN
F 1 "8pF" H 992 3455 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 900 3500 50  0001 C CNN
F 3 "https://www.yageo.com/upload/media/product/productsearch/datasheet/mlcc/UPY-GP_NP0_16V-to-50V_18.pdf" H 900 3500 50  0001 C CNN
F 4 "CAP CER 8PF 50V NPO 0603" H 900 3500 50  0001 C CNN "Description"
F 5 "Yageo" H 900 3500 50  0001 C CNN "Manufacturer"
F 6 "CC0603CRNPO9BN8R0" H 900 3500 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 900 3500 50  0001 C CNN "Supplier"
F 8 "311-3864-1-ND" H 900 3500 50  0001 C CNN "Supplier PN"
	1    900  3500
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C22
U 1 1 5F950FD6
P 1500 3500
F 0 "C22" H 1592 3546 50  0000 L CNN
F 1 "8pF" H 1592 3455 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1500 3500 50  0001 C CNN
F 3 "https://www.yageo.com/upload/media/product/productsearch/datasheet/mlcc/UPY-GP_NP0_16V-to-50V_18.pdf" H 1500 3500 50  0001 C CNN
F 4 "CAP CER 8PF 50V NPO 0603" H 1500 3500 50  0001 C CNN "Description"
F 5 "Yageo" H 1500 3500 50  0001 C CNN "Manufacturer"
F 6 "CC0603CRNPO9BN8R0" H 1500 3500 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1500 3500 50  0001 C CNN "Supplier"
F 8 "311-3864-1-ND" H 1500 3500 50  0001 C CNN "Supplier PN"
	1    1500 3500
	1    0    0    -1  
$EndComp
Wire Wire Line
	1050 3150 900  3150
Wire Wire Line
	900  3150 900  3400
Wire Wire Line
	1350 3150 1500 3150
Wire Wire Line
	1500 3150 1500 3400
Wire Wire Line
	2650 2900 1500 2900
Wire Wire Line
	1500 2900 1500 3150
Connection ~ 1500 3150
Wire Wire Line
	2650 2800 900  2800
Wire Wire Line
	900  2800 900  3150
Connection ~ 900  3150
$Comp
L power:GND #PWR0105
U 1 1 5F95AD21
P 900 3600
F 0 "#PWR0105" H 900 3350 50  0001 C CNN
F 1 "GND" H 905 3427 50  0000 C CNN
F 2 "" H 900 3600 50  0001 C CNN
F 3 "" H 900 3600 50  0001 C CNN
	1    900  3600
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0106
U 1 1 5F95B4D9
P 1500 3600
F 0 "#PWR0106" H 1500 3350 50  0001 C CNN
F 1 "GND" H 1505 3427 50  0000 C CNN
F 2 "" H 1500 3600 50  0001 C CNN
F 3 "" H 1500 3600 50  0001 C CNN
	1    1500 3600
	1    0    0    -1  
$EndComp
NoConn ~ 1200 3350
NoConn ~ 1200 2950
$Comp
L Project:M24M01-RMN U6
U 1 1 5F950792
P 3850 6350
F 0 "U6" H 3650 6600 50  0000 C CNN
F 1 "M24M01-RMN" H 4150 6100 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 3850 6700 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/group0/cb/91/ba/7d/0b/c1/4d/f6/CD00147128/files/CD00147128.pdf/jcr:content/translations/en.CD00147128.pdf" H 3900 5850 50  0001 C CNN
F 4 "IC EEPROM 1M I2C 1MHZ 8SO" H 3850 6350 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3850 6350 50  0001 C CNN "Manufacturer"
F 6 "M24M01-RMN6TP" H 3850 6350 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3850 6350 50  0001 C CNN "Supplier"
F 8 "497-6351-1-ND" H 3850 6350 50  0001 C CNN "Supplier PN"
	1    3850 6350
	1    0    0    -1  
$EndComp
$Comp
L Device:L_Small L3
U 1 1 5F951860
P 4400 850
F 0 "L3" V 4600 850 50  0000 C CNN
F 1 "600 Ohm" V 4500 850 50  0000 C CNN
F 2 "Inductor_SMD:L_0603_1608Metric" H 4400 850 50  0001 C CNN
F 3 "https://www.yuden.co.jp/productdata/catalog/mlci07_e.pdf" H 4400 850 50  0001 C CNN
F 4 "FERRITE BEAD 600 OHM 0603 1LN" H 4400 850 50  0001 C CNN "Description"
F 5 "Taiyo Yuden" H 4400 850 50  0001 C CNN "Manufacturer"
F 6 "BK1608HS601-T" H 4400 850 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4400 850 50  0001 C CNN "Supplier"
F 8 "587-1874-1-ND" H 4400 850 50  0001 C CNN "Supplier PN"
	1    4400 850 
	0    -1   -1   0   
$EndComp
$Comp
L power:+3.3V #PWR0107
U 1 1 5F96416A
P 4700 850
F 0 "#PWR0107" H 4700 700 50  0001 C CNN
F 1 "+3.3V" H 4715 1023 50  0000 C CNN
F 2 "" H 4700 850 50  0001 C CNN
F 3 "" H 4700 850 50  0001 C CNN
	1    4700 850 
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C14
U 1 1 5F9656F7
P 4200 1200
F 0 "C14" H 4292 1246 50  0000 L CNN
F 1 "0.1uF" H 4292 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4200 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 4200 1200 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 4200 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 4200 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 4200 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4200 1200 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 4200 1200 50  0001 C CNN "Supplier PN"
	1    4200 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C15
U 1 1 5F965D4A
P 4600 1200
F 0 "C15" H 4692 1246 50  0000 L CNN
F 1 "1uF" H 4692 1155 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4600 1200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 4600 1200 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 4600 1200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 4600 1200 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 4600 1200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4600 1200 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 4600 1200 50  0001 C CNN "Supplier PN"
	1    4600 1200
	1    0    0    -1  
$EndComp
Wire Wire Line
	3650 1500 3650 1000
Wire Wire Line
	3650 1000 4200 1000
Wire Wire Line
	4200 1100 4200 1000
Connection ~ 4200 1000
Wire Wire Line
	4200 1000 4600 1000
Wire Wire Line
	4600 1100 4600 1000
$Comp
L power:GND #PWR0108
U 1 1 5F969C61
P 4200 1350
F 0 "#PWR0108" H 4200 1100 50  0001 C CNN
F 1 "GND" H 4205 1177 50  0000 C CNN
F 2 "" H 4200 1350 50  0001 C CNN
F 3 "" H 4200 1350 50  0001 C CNN
	1    4200 1350
	1    0    0    -1  
$EndComp
Wire Wire Line
	4200 1300 4200 1350
Wire Wire Line
	4600 1300 4600 1350
Wire Wire Line
	4600 1350 4200 1350
Connection ~ 4200 1350
Wire Wire Line
	4500 850  4700 850 
Wire Wire Line
	4300 850  4200 850 
Wire Wire Line
	4200 850  4200 1000
$Comp
L Device:C_Small C13
U 1 1 5F97FD14
P 2750 1150
F 0 "C13" H 2842 1196 50  0000 L CNN
F 1 "0.1uF" H 2842 1105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2750 1150 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 2750 1150 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 2750 1150 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 2750 1150 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 2750 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2750 1150 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 2750 1150 50  0001 C CNN "Supplier PN"
	1    2750 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C12
U 1 1 5F980A35
P 2350 1150
F 0 "C12" H 2442 1196 50  0000 L CNN
F 1 "0.1uF" H 2442 1105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2350 1150 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 2350 1150 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 2350 1150 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 2350 1150 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 2350 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2350 1150 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 2350 1150 50  0001 C CNN "Supplier PN"
	1    2350 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C11
U 1 1 5F980D6F
P 1950 1150
F 0 "C11" H 2042 1196 50  0000 L CNN
F 1 "0.1uF" H 2042 1105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1950 1150 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1950 1150 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1950 1150 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1950 1150 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1950 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1950 1150 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1950 1150 50  0001 C CNN "Supplier PN"
	1    1950 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C10
U 1 1 5F9811DA
P 1550 1150
F 0 "C10" H 1642 1196 50  0000 L CNN
F 1 "0.1uF" H 1642 1105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1550 1150 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1550 1150 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1550 1150 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1550 1150 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1550 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1550 1150 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1550 1150 50  0001 C CNN "Supplier PN"
	1    1550 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C9
U 1 1 5F981EC3
P 1150 1150
F 0 "C9" H 1242 1196 50  0000 L CNN
F 1 "0.1uF" H 1242 1105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1150 1150 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1150 1150 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1150 1150 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1150 1150 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1150 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1150 1150 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1150 1150 50  0001 C CNN "Supplier PN"
	1    1150 1150
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0109
U 1 1 5F98252A
P 750 1300
F 0 "#PWR0109" H 750 1050 50  0001 C CNN
F 1 "GND" H 755 1127 50  0000 C CNN
F 2 "" H 750 1300 50  0001 C CNN
F 3 "" H 750 1300 50  0001 C CNN
	1    750  1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	1150 1250 1150 1300
Wire Wire Line
	1150 1300 1550 1300
Wire Wire Line
	2750 1300 2750 1250
Wire Wire Line
	2350 1250 2350 1300
Connection ~ 2350 1300
Wire Wire Line
	2350 1300 2750 1300
Wire Wire Line
	1950 1250 1950 1300
Connection ~ 1950 1300
Wire Wire Line
	1950 1300 2350 1300
Wire Wire Line
	1550 1250 1550 1300
Connection ~ 1550 1300
Wire Wire Line
	1550 1300 1950 1300
$Comp
L power:+3.3V #PWR0110
U 1 1 5F988BE8
P 750 1000
F 0 "#PWR0110" H 750 850 50  0001 C CNN
F 1 "+3.3V" H 765 1173 50  0000 C CNN
F 2 "" H 750 1000 50  0001 C CNN
F 3 "" H 750 1000 50  0001 C CNN
	1    750  1000
	1    0    0    -1  
$EndComp
Wire Wire Line
	1150 1050 1150 1000
Wire Wire Line
	1150 1000 1550 1000
Wire Wire Line
	3550 1000 3550 1500
Wire Wire Line
	1550 1050 1550 1000
Connection ~ 1550 1000
Wire Wire Line
	1550 1000 1950 1000
Wire Wire Line
	1950 1050 1950 1000
Connection ~ 1950 1000
Wire Wire Line
	1950 1000 2350 1000
Wire Wire Line
	2350 1050 2350 1000
Connection ~ 2350 1000
Wire Wire Line
	2350 1000 2750 1000
Wire Wire Line
	2750 1050 2750 1000
Connection ~ 2750 1000
Wire Wire Line
	3450 1500 3450 1000
Connection ~ 3450 1000
Wire Wire Line
	3450 1000 3550 1000
Wire Wire Line
	3350 1500 3350 1000
Wire Wire Line
	2750 1000 3150 1000
Connection ~ 3350 1000
Wire Wire Line
	3350 1000 3450 1000
Wire Wire Line
	3250 1500 3250 1000
Connection ~ 3250 1000
Wire Wire Line
	3250 1000 3350 1000
Wire Wire Line
	3150 1500 3150 1000
Connection ~ 3150 1000
Wire Wire Line
	3150 1000 3250 1000
$Comp
L Device:C_Small C8
U 1 1 5F9A4479
P 750 1150
F 0 "C8" H 842 1196 50  0000 L CNN
F 1 "10uF" H 842 1105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 750 1150 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 750 1150 50  0001 C CNN
F 4 "CAP CER 10UF 25V X5R 0805" H 750 1150 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 750 1150 50  0001 C CNN "Manufacturer"
F 6 "CL21A106KAYNNNG" H 750 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 750 1150 50  0001 C CNN "Supplier"
F 8 "1276-6454-1-ND" H 750 1150 50  0001 C CNN "Supplier PN"
	1    750  1150
	1    0    0    -1  
$EndComp
Wire Wire Line
	750  1050 750  1000
Wire Wire Line
	1150 1000 750  1000
Connection ~ 1150 1000
Connection ~ 750  1000
Wire Wire Line
	1150 1300 750  1300
Connection ~ 1150 1300
Wire Wire Line
	750  1250 750  1300
Connection ~ 750  1300
Text Notes 1150 900  0    50   ~ 0
0.1uF caps are for each power pin
$Comp
L Device:C_Small C20
U 1 1 5F9AEFEC
P 2400 2250
F 0 "C20" H 2492 2296 50  0000 L CNN
F 1 "4.7uF" H 2492 2205 50  0000 L CNN
F 2 "Capacitor_SMD:C_1206_3216Metric" H 2400 2250 50  0001 C CNN
F 3 "https://www.yageo.com/upload/media/product/productsearch/datasheet/mlcc/UPY-GPHC_X7R_6.3V-to-50V_18.pdf" H 2400 2250 50  0001 C CNN
F 4 "CAP CER 4.7UF 25V X7R 1206" H 2400 2250 50  0001 C CNN "Description"
F 5 "Yageo" H 2400 2250 50  0001 C CNN "Manufacturer"
F 6 "CC1206KKX7R8BB475" H 2400 2250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2400 2250 50  0001 C CNN "Supplier"
F 8 "311-1961-1-ND" H 2400 2250 50  0001 C CNN "Supplier PN"
	1    2400 2250
	1    0    0    -1  
$EndComp
Text Notes 1800 2650 0    50   ~ 0
VCAP needs\n<1ohm ESR
$Comp
L power:GND #PWR0111
U 1 1 5F9AF7F7
P 2400 2350
F 0 "#PWR0111" H 2400 2100 50  0001 C CNN
F 1 "GND" H 2405 2177 50  0000 C CNN
F 2 "" H 2400 2350 50  0001 C CNN
F 3 "" H 2400 2350 50  0001 C CNN
	1    2400 2350
	1    0    0    -1  
$EndComp
Wire Wire Line
	2650 2100 2400 2100
Wire Wire Line
	2400 2100 2400 2150
$Comp
L Device:R_Small R1
U 1 1 5F9B3310
P 2100 2100
F 0 "R1" H 2159 2146 50  0000 L CNN
F 1 "10K" H 2159 2055 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 2100 2100 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2100 2100 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2100 2100 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2100 2100 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2100 2100 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2100 2100 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2100 2100 50  0001 C CNN "Supplier PN"
	1    2100 2100
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0112
U 1 1 5F9B41EF
P 2100 2200
F 0 "#PWR0112" H 2100 1950 50  0001 C CNN
F 1 "GND" H 2105 2027 50  0000 C CNN
F 2 "" H 2100 2200 50  0001 C CNN
F 3 "" H 2100 2200 50  0001 C CNN
	1    2100 2200
	1    0    0    -1  
$EndComp
Wire Wire Line
	2650 1900 2100 1900
Wire Wire Line
	2100 1900 2100 2000
$Comp
L power:GND #PWR0113
U 1 1 5F9BB459
P 3550 5100
F 0 "#PWR0113" H 3550 4850 50  0001 C CNN
F 1 "GND" H 3555 4927 50  0000 C CNN
F 2 "" H 3550 5100 50  0001 C CNN
F 3 "" H 3550 5100 50  0001 C CNN
	1    3550 5100
	1    0    0    -1  
$EndComp
Wire Wire Line
	3150 5000 3150 5100
Wire Wire Line
	3150 5100 3250 5100
Wire Wire Line
	3250 5000 3250 5100
Connection ~ 3250 5100
Wire Wire Line
	3250 5100 3350 5100
Wire Wire Line
	3350 5000 3350 5100
Connection ~ 3350 5100
Wire Wire Line
	3350 5100 3450 5100
Wire Wire Line
	3450 5000 3450 5100
Connection ~ 3450 5100
Wire Wire Line
	3450 5100 3550 5100
Wire Wire Line
	3550 5000 3550 5100
Connection ~ 3550 5100
$Comp
L Device:C_Small C19
U 1 1 5F9CAEBD
P 1700 1900
F 0 "C19" H 1792 1946 50  0000 L CNN
F 1 "0.1uF" H 1792 1855 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1700 1900 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1700 1900 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1700 1900 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1700 1900 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1700 1900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1700 1900 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1700 1900 50  0001 C CNN "Supplier PN"
	1    1700 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	2650 1700 1700 1700
Wire Wire Line
	1700 1700 1700 1800
$Comp
L power:GND #PWR0114
U 1 1 5F9CF1A9
P 1700 2000
F 0 "#PWR0114" H 1700 1750 50  0001 C CNN
F 1 "GND" H 1705 1827 50  0000 C CNN
F 2 "" H 1700 2000 50  0001 C CNN
F 3 "" H 1700 2000 50  0001 C CNN
	1    1700 2000
	1    0    0    -1  
$EndComp
Text Label 1400 1700 0    50   ~ 0
RESET
Wire Wire Line
	1700 1700 1400 1700
Connection ~ 1700 1700
Text Label 1950 5900 0    50   ~ 0
RESET
Text Label 4150 1900 0    50   ~ 0
BUZZ_DIN
Wire Wire Line
	4050 1900 4750 1900
Text Label 4150 2100 0    50   ~ 0
DISP_~CS
Text Label 4150 2200 0    50   ~ 0
DISP_SPI_SCK
Text Label 4150 2300 0    50   ~ 0
DISP_D~C
Text Label 4150 2400 0    50   ~ 0
DISP_SPI_MOSI
Wire Wire Line
	4050 2400 4750 2400
Wire Wire Line
	4050 2300 4750 2300
Wire Wire Line
	4050 2200 4750 2200
Wire Wire Line
	4050 2100 4750 2100
Text Label 2000 3700 0    50   ~ 0
DISP_~RES
Text Label 2000 3400 0    50   ~ 0
USB_DRIVE_VBUS
Text Label 2000 3500 0    50   ~ 0
USB_VBUS_OC
Text Label 4150 3500 0    50   ~ 0
SENSOR_~INT
Wire Wire Line
	4050 3500 4750 3500
Text Label 4150 4400 0    50   ~ 0
I2C2_SCL
Wire Wire Line
	4050 4400 4750 4400
Text Label 2000 4200 0    50   ~ 0
RELAY_ENLG
Text Label 4150 2500 0    50   ~ 0
ENC_CH1
Text Label 4150 2600 0    50   ~ 0
ENC_CH2
Wire Wire Line
	4050 2500 4750 2500
Wire Wire Line
	4050 2600 4750 2600
Text Label 4150 2700 0    50   ~ 0
USART1_RX
Wire Wire Line
	4050 2700 4750 2700
Text Label 4150 2800 0    50   ~ 0
USB_OTG_FS_D-
Text Label 4150 2900 0    50   ~ 0
USB_OTG_FS_D+
Wire Wire Line
	4050 2800 4750 2800
Wire Wire Line
	4050 2900 4750 2900
Text Label 4150 3000 0    50   ~ 0
JTMS_SWDIO
Text Label 4150 3100 0    50   ~ 0
JCLK_SWCLK
Text Label 4150 3200 0    50   ~ 0
JTDI
Wire Wire Line
	4050 3000 4750 3000
Wire Wire Line
	4050 3100 4750 3100
Wire Wire Line
	4050 3200 4750 3200
Text Label 4150 3700 0    50   ~ 0
JTDO_SWO
Wire Wire Line
	4050 3700 4750 3700
Text Label 4150 3900 0    50   ~ 0
KEY_~INT
Text Label 4150 4000 0    50   ~ 0
USART1_TX
Text Label 4150 4200 0    50   ~ 0
I2C1_SCL
Text Label 4150 4300 0    50   ~ 0
I2C2_SDA
Wire Wire Line
	4050 4300 4750 4300
Wire Wire Line
	4050 4000 4750 4000
Wire Wire Line
	4050 3900 4750 3900
Wire Wire Line
	1850 5900 2500 5900
NoConn ~ 1850 6600
$Comp
L power:GND #PWR014
U 1 1 5FB2CF53
P 1250 7150
F 0 "#PWR014" H 1250 6900 50  0001 C CNN
F 1 "GND" H 1255 6977 50  0000 C CNN
F 2 "" H 1250 7150 50  0001 C CNN
F 3 "" H 1250 7150 50  0001 C CNN
	1    1250 7150
	1    0    0    -1  
$EndComp
Wire Wire Line
	1350 7100 1250 7100
Wire Wire Line
	1250 7100 1250 7150
Connection ~ 1250 7100
$Comp
L Project:ESDALC6V1W5 D11
U 1 1 5F97AB57
P 2800 6250
F 0 "D11" H 2800 6642 50  0000 C CNN
F 1 "ESDALC6V1W5" H 2800 6551 50  0000 C CNN
F 2 "lib_fp:SOT323-5L" H 2800 5875 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/32/30/02/e6/ac/0f/46/c2/CD00002946.pdf/files/CD00002946.pdf/jcr:content/translations/en.CD00002946.pdf" H 2800 6300 50  0001 C CNN
F 4 "TVS DIODE 3V SOT323-5" H 2800 6250 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 2800 6250 50  0001 C CNN "Manufacturer"
F 6 "ESDALC6V1W5" H 2800 6250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2800 6250 50  0001 C CNN "Supplier"
F 8 "497-7231-1-ND" H 2800 6250 50  0001 C CNN "Supplier PN"
	1    2800 6250
	1    0    0    -1  
$EndComp
$Comp
L Project:ESDALC6V1W5 D12
U 1 1 5F97DF98
P 2800 6950
F 0 "D12" H 2800 7342 50  0000 C CNN
F 1 "ESDALC6V1W5" H 2800 7251 50  0000 C CNN
F 2 "lib_fp:SOT323-5L" H 2800 6575 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/32/30/02/e6/ac/0f/46/c2/CD00002946.pdf/files/CD00002946.pdf/jcr:content/translations/en.CD00002946.pdf" H 2800 7000 50  0001 C CNN
F 4 "TVS DIODE 3V SOT323-5" H 2800 6950 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 2800 6950 50  0001 C CNN "Manufacturer"
F 6 "ESDALC6V1W5" H 2800 6950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2800 6950 50  0001 C CNN "Supplier"
F 8 "497-7231-1-ND" H 2800 6950 50  0001 C CNN "Supplier PN"
	1    2800 6950
	1    0    0    -1  
$EndComp
Wire Wire Line
	1850 6100 2600 6100
Wire Wire Line
	1850 6200 2600 6200
Wire Wire Line
	1850 6300 2600 6300
Wire Wire Line
	1850 6400 2600 6400
Wire Wire Line
	1850 6800 2450 6800
Wire Wire Line
	1850 6900 2400 6900
NoConn ~ 2600 7100
$Comp
L power:GND #PWR012
U 1 1 5F9D5CF7
P 3050 7000
F 0 "#PWR012" H 3050 6750 50  0001 C CNN
F 1 "GND" H 3055 6827 50  0000 C CNN
F 2 "" H 3050 7000 50  0001 C CNN
F 3 "" H 3050 7000 50  0001 C CNN
	1    3050 7000
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR011
U 1 1 5F9D628B
P 3050 6300
F 0 "#PWR011" H 3050 6050 50  0001 C CNN
F 1 "GND" H 3055 6127 50  0000 C CNN
F 2 "" H 3050 6300 50  0001 C CNN
F 3 "" H 3050 6300 50  0001 C CNN
	1    3050 6300
	1    0    0    -1  
$EndComp
Wire Wire Line
	3000 6250 3050 6250
Wire Wire Line
	3050 6250 3050 6300
Wire Wire Line
	3000 6950 3050 6950
Wire Wire Line
	3050 6950 3050 7000
Text Label 1950 6100 0    50   ~ 0
JCLK_SWCLK
Text Label 1950 6200 0    50   ~ 0
JTMS_SWDIO
Text Label 1950 6300 0    50   ~ 0
JTDO_SWO
Text Label 1950 6400 0    50   ~ 0
JTDI
Text Label 1950 6800 0    50   ~ 0
USART1_RX
Text Label 1950 6900 0    50   ~ 0
USART1_TX
Wire Wire Line
	2400 6900 2400 7000
Wire Wire Line
	2400 7000 2600 7000
Wire Wire Line
	2450 6800 2450 6900
Wire Wire Line
	2450 6900 2600 6900
Wire Wire Line
	2500 5900 2500 6800
Wire Wire Line
	2500 6800 2600 6800
Text Notes 1900 7400 0    50   ~ 0
Can swap connections on ESD\nprotection to optimize routing
NoConn ~ 1350 5700
Text Label 5850 3550 0    50   ~ 0
I2C2_SCL
Text Label 5850 3450 0    50   ~ 0
I2C2_SDA
Text Label 5850 3800 0    50   ~ 0
SENSOR_~INT
Text Label 5850 4350 0    50   ~ 0
USB_OTG_FS_D-
Text Label 5850 4250 0    50   ~ 0
USB_OTG_FS_D+
Text Label 5850 4050 0    50   ~ 0
USB_DRIVE_VBUS
Text Label 5850 4150 0    50   ~ 0
USB_VBUS_OC
Text Label 5850 2450 0    50   ~ 0
SENSOR_BTN
Text Label 5850 2550 0    50   ~ 0
FOOTSWITCH
Text Label 5850 3700 0    50   ~ 0
SENSOR_BTN
Text Label 5850 3900 0    50   ~ 0
FOOTSWITCH
Wire Wire Line
	6500 2450 5850 2450
Wire Wire Line
	5850 2550 6500 2550
Text Label 5850 1850 0    50   ~ 0
I2C1_SDA
Text Label 5850 1950 0    50   ~ 0
I2C1_SCL
Wire Wire Line
	5850 1950 6500 1950
Wire Wire Line
	5850 1850 6500 1850
Text Label 5850 2050 0    50   ~ 0
KEY_~INT
Wire Wire Line
	5850 2050 6500 2050
Text Notes 4450 4600 0    50   ~ 0
I2C2 has pull-ups as part\nof main-board-front
Text Label 5850 950  0    50   ~ 0
RELAY_SFLT
Text Label 5850 850  0    50   ~ 0
RELAY_ENLG
Wire Wire Line
	6500 850  5850 850 
Wire Wire Line
	6500 950  5850 950 
Text Label 2000 4100 0    50   ~ 0
RELAY_SFLT
Wire Wire Line
	2650 3400 2000 3400
Wire Wire Line
	2650 3500 2000 3500
Wire Wire Line
	2650 3700 2000 3700
Wire Wire Line
	2650 4100 2000 4100
Wire Wire Line
	2650 4200 2000 4200
$Comp
L power:+3.3V #PWR06
U 1 1 5FA693EA
P 3850 5800
F 0 "#PWR06" H 3850 5650 50  0001 C CNN
F 1 "+3.3V" H 3865 5973 50  0000 C CNN
F 2 "" H 3850 5800 50  0001 C CNN
F 3 "" H 3850 5800 50  0001 C CNN
	1    3850 5800
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C23
U 1 1 5FA699B6
P 4000 5900
F 0 "C23" V 4150 5750 50  0000 L CNN
F 1 "0.1uF" V 4050 5650 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4000 5900 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 4000 5900 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 4000 5900 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 4000 5900 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 4000 5900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4000 5900 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 4000 5900 50  0001 C CNN "Supplier PN"
	1    4000 5900
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR07
U 1 1 5FA6B086
P 4100 5900
F 0 "#PWR07" H 4100 5650 50  0001 C CNN
F 1 "GND" H 4105 5727 50  0000 C CNN
F 2 "" H 4100 5900 50  0001 C CNN
F 3 "" H 4100 5900 50  0001 C CNN
	1    4100 5900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3850 6050 3850 5900
Wire Wire Line
	3900 5900 3850 5900
Connection ~ 3850 5900
Wire Wire Line
	3850 5900 3850 5800
$Comp
L power:GND #PWR010
U 1 1 5FA90C2E
P 3850 6650
F 0 "#PWR010" H 3850 6400 50  0001 C CNN
F 1 "GND" H 3855 6477 50  0000 C CNN
F 2 "" H 3850 6650 50  0001 C CNN
F 3 "" H 3850 6650 50  0001 C CNN
	1    3850 6650
	1    0    0    -1  
$EndComp
Text Label 4150 4100 0    50   ~ 0
I2C1_SDA
Text Label 4650 6250 2    50   ~ 0
I2C1_SDA
Text Label 4650 6350 2    50   ~ 0
I2C1_SCL
$Comp
L Device:R_Small R2
U 1 1 5FA91E20
P 4900 3950
F 0 "R2" H 4959 3996 50  0000 L CNN
F 1 "2K" H 4959 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 4900 3950 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 4900 3950 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 4900 3950 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 4900 3950 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 4900 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4900 3950 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 4900 3950 50  0001 C CNN "Supplier PN"
	1    4900 3950
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R3
U 1 1 5FA9230A
P 5150 3950
F 0 "R3" H 5209 3996 50  0000 L CNN
F 1 "2K" H 5209 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 5150 3950 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 5150 3950 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 5150 3950 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 5150 3950 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 5150 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5150 3950 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 5150 3950 50  0001 C CNN "Supplier PN"
	1    5150 3950
	1    0    0    -1  
$EndComp
Wire Wire Line
	4900 4100 4900 4050
Wire Wire Line
	4050 4100 4900 4100
Wire Wire Line
	5150 4200 5150 4050
Wire Wire Line
	4050 4200 5150 4200
$Comp
L power:+3.3V #PWR05
U 1 1 5FAB8BA0
P 5150 3750
F 0 "#PWR05" H 5150 3600 50  0001 C CNN
F 1 "+3.3V" H 5165 3923 50  0000 C CNN
F 2 "" H 5150 3750 50  0001 C CNN
F 3 "" H 5150 3750 50  0001 C CNN
	1    5150 3750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5150 3850 5150 3800
Wire Wire Line
	4900 3850 4900 3800
Wire Wire Line
	4900 3800 5150 3800
Connection ~ 5150 3800
Wire Wire Line
	5150 3800 5150 3750
Wire Wire Line
	4250 6450 4500 6450
$Comp
L power:GND #PWR08
U 1 1 5FB2CC3B
P 3400 6500
F 0 "#PWR08" H 3400 6250 50  0001 C CNN
F 1 "GND" H 3405 6327 50  0000 C CNN
F 2 "" H 3400 6500 50  0001 C CNN
F 3 "" H 3400 6500 50  0001 C CNN
	1    3400 6500
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR09
U 1 1 5FB2D122
P 4500 6500
F 0 "#PWR09" H 4500 6250 50  0001 C CNN
F 1 "GND" H 4505 6327 50  0000 C CNN
F 2 "" H 4500 6500 50  0001 C CNN
F 3 "" H 4500 6500 50  0001 C CNN
	1    4500 6500
	1    0    0    -1  
$EndComp
Wire Wire Line
	4500 6450 4500 6500
Wire Wire Line
	3450 6350 3400 6350
Wire Wire Line
	3400 6350 3400 6450
Wire Wire Line
	3450 6450 3400 6450
Connection ~ 3400 6450
Wire Wire Line
	3400 6450 3400 6500
$Comp
L Project:PAM8904EJP U5
U 1 1 5FB60707
P 9100 1550
F 0 "U5" H 8850 2050 50  0000 C CNN
F 1 "PAM8904EJP" H 9400 1000 50  0000 C CNN
F 2 "lib_fp:U-QFN3030-12_Type-A" H 9100 550 50  0001 C CNN
F 3 "https://www.diodes.com/assets/Datasheets/PAM8904E.pdf" H 9100 1550 50  0001 C CNN
F 4 "AUDIO HIGH VOLT U-QFN3030-16" H 9100 1550 50  0001 C CNN "Description"
F 5 "Diodes Incorporated" H 9100 1550 50  0001 C CNN "Manufacturer"
F 6 "PAM8904EJER" H 9100 1550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9100 1550 50  0001 C CNN "Supplier"
F 8 "31-PAM8904EJERCT-ND" H 9100 1550 50  0001 C CNN "Supplier PN"
	1    9100 1550
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
P 9300 900
F 0 "C7" V 9071 900 50  0000 C CNN
F 1 "2.2uF" V 9162 900 50  0000 C CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 9300 900 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9300 900 50  0001 C CNN
F 4 "CAP CER 2.2UF 25V X5R 0805" H 9300 900 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9300 900 50  0001 C CNN "Manufacturer"
F 6 "CL21A225KAFNNNG" H 9300 900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9300 900 50  0001 C CNN "Supplier"
F 8 "1276-6458-1-ND" H 9300 900 50  0001 C CNN "Supplier PN"
	1    9300 900 
	0    1    1    0   
$EndComp
$Comp
L Device:C_Small C17
U 1 1 5FB60729
P 9750 1400
F 0 "C17" H 9842 1446 50  0000 L CNN
F 1 "1uF" H 9842 1355 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 9750 1400 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9750 1400 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 9750 1400 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9750 1400 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 9750 1400 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9750 1400 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 9750 1400 50  0001 C CNN "Supplier PN"
	1    9750 1400
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C18
U 1 1 5FB60734
P 9750 1650
F 0 "C18" H 9842 1696 50  0000 L CNN
F 1 "1uF" H 9842 1605 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 9750 1650 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 9750 1650 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 9750 1650 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 9750 1650 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 9750 1650 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 9750 1650 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 9750 1650 50  0001 C CNN "Supplier PN"
	1    9750 1650
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C16
U 1 1 5FB6073F
P 10100 1300
F 0 "C16" H 10192 1346 50  0000 L CNN
F 1 "2.2uF" H 10192 1255 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 10100 1300 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 10100 1300 50  0001 C CNN
F 4 "CAP CER 2.2UF 25V X5R 0805" H 10100 1300 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 10100 1300 50  0001 C CNN "Manufacturer"
F 6 "CL21A225KAFNNNG" H 10100 1300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 10100 1300 50  0001 C CNN "Supplier"
F 8 "1276-6458-1-ND" H 10100 1300 50  0001 C CNN "Supplier PN"
	1    10100 1300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR04
U 1 1 5FB60745
P 9100 2150
F 0 "#PWR04" H 9100 1900 50  0001 C CNN
F 1 "GND" H 9105 1977 50  0000 C CNN
F 2 "" H 9100 2150 50  0001 C CNN
F 3 "" H 9100 2150 50  0001 C CNN
	1    9100 2150
	1    0    0    -1  
$EndComp
Wire Wire Line
	9100 1000 9100 900 
Wire Wire Line
	9200 900  9100 900 
Connection ~ 9100 900 
Wire Wire Line
	9100 900  9100 800 
$Comp
L power:GND #PWR02
U 1 1 5FB6077A
P 9500 900
F 0 "#PWR02" H 9500 650 50  0001 C CNN
F 1 "GND" H 9505 727 50  0000 C CNN
F 2 "" H 9500 900 50  0001 C CNN
F 3 "" H 9500 900 50  0001 C CNN
	1    9500 900 
	1    0    0    -1  
$EndComp
Wire Wire Line
	9400 900  9500 900 
Wire Wire Line
	9500 1350 9600 1350
Wire Wire Line
	9600 1350 9600 1300
Wire Wire Line
	9600 1300 9750 1300
Wire Wire Line
	9500 1450 9600 1450
Wire Wire Line
	9600 1450 9600 1500
Wire Wire Line
	9600 1500 9750 1500
Wire Wire Line
	9500 1600 9600 1600
Wire Wire Line
	9600 1600 9600 1550
Wire Wire Line
	9600 1550 9750 1550
Wire Wire Line
	9500 1700 9600 1700
Wire Wire Line
	9600 1700 9600 1750
Wire Wire Line
	9600 1750 9750 1750
Wire Wire Line
	9500 1200 10100 1200
$Comp
L power:GND #PWR03
U 1 1 5FB6078E
P 10100 1400
F 0 "#PWR03" H 10100 1150 50  0001 C CNN
F 1 "GND" H 10105 1227 50  0000 C CNN
F 2 "" H 10100 1400 50  0001 C CNN
F 3 "" H 10100 1400 50  0001 C CNN
	1    10100 1400
	1    0    0    -1  
$EndComp
Wire Wire Line
	9500 1850 9800 1850
Wire Wire Line
	9800 1950 9500 1950
$Comp
L power:+3.3V #PWR01
U 1 1 5FBD6808
P 9100 800
F 0 "#PWR01" H 9100 650 50  0001 C CNN
F 1 "+3.3V" H 9115 973 50  0000 C CNN
F 2 "" H 9100 800 50  0001 C CNN
F 3 "" H 9100 800 50  0001 C CNN
	1    9100 800 
	1    0    0    -1  
$EndComp
Text Label 2000 4000 0    50   ~ 0
LED_CLK
Text Label 2000 3900 0    50   ~ 0
LED_~OE
Text Label 4150 4800 0    50   ~ 0
LED_SDI
Text Label 4150 4700 0    50   ~ 0
LED_LE
Wire Wire Line
	2000 4000 2650 4000
Wire Wire Line
	2000 3900 2650 3900
Wire Wire Line
	4050 4700 4750 4700
Wire Wire Line
	4050 4800 4750 4800
Text Label 5850 3000 0    50   ~ 0
LED_~OE
Text Label 5850 2800 0    50   ~ 0
LED_CLK
Text Label 5850 2900 0    50   ~ 0
LED_LE
Text Label 5850 2700 0    50   ~ 0
LED_SDI
Wire Wire Line
	6500 2700 5850 2700
Wire Wire Line
	6500 2800 5850 2800
Wire Wire Line
	6500 2900 5850 2900
Wire Wire Line
	6500 3000 5850 3000
NoConn ~ 4050 2000
NoConn ~ 4050 3400
NoConn ~ 4050 3800
NoConn ~ 4050 4500
NoConn ~ 4050 4600
NoConn ~ 2650 4800
NoConn ~ 2650 4700
NoConn ~ 2650 4600
NoConn ~ 2650 4500
NoConn ~ 2650 4400
NoConn ~ 2650 4300
NoConn ~ 2650 3800
NoConn ~ 2650 3600
NoConn ~ 2650 3300
NoConn ~ 2650 3100
$Comp
L power:PWR_FLAG #FLG0108
U 1 1 5FB7BB20
P 3650 1000
F 0 "#FLG0108" H 3650 1075 50  0001 C CNN
F 1 "PWR_FLAG" H 3650 1173 50  0001 C CNN
F 2 "" H 3650 1000 50  0001 C CNN
F 3 "~" H 3650 1000 50  0001 C CNN
	1    3650 1000
	1    0    0    -1  
$EndComp
Connection ~ 3650 1000
$Comp
L power:PWR_FLAG #FLG0109
U 1 1 5FB7C012
P 2400 2100
F 0 "#FLG0109" H 2400 2175 50  0001 C CNN
F 1 "PWR_FLAG" H 2400 2273 50  0001 C CNN
F 2 "" H 2400 2100 50  0001 C CNN
F 3 "~" H 2400 2100 50  0001 C CNN
	1    2400 2100
	1    0    0    -1  
$EndComp
Connection ~ 2400 2100
NoConn ~ 1100 7100
Wire Wire Line
	5850 4350 6500 4350
Wire Wire Line
	5850 4250 6500 4250
Wire Wire Line
	5850 4150 6500 4150
Wire Wire Line
	6500 4050 5850 4050
Wire Wire Line
	5850 3900 6500 3900
Wire Wire Line
	5850 3800 6500 3800
Wire Wire Line
	5850 3700 6500 3700
Wire Wire Line
	5850 3550 6500 3550
Wire Wire Line
	6500 3450 5850 3450
$Sheet
S 6500 3350 1400 1100
U 5FA19AFF
F0 "Front External Connections" 50
F1 "main-board-front.sch" 50
F2 "EXT_I2C_SDA" B L 6500 3450 50 
F3 "EXT_I2C_SCL" B L 6500 3550 50 
F4 "SENSOR_BTN" O L 6500 3700 50 
F5 "SENSOR_INT" O L 6500 3800 50 
F6 "FOOTSWITCH" O L 6500 3900 50 
F7 "USB_DRIVE_VBUS" I L 6500 4050 50 
F8 "USB_VBUS_OC" O L 6500 4150 50 
F9 "USB_D+" B L 6500 4250 50 
F10 "USB_D-" B L 6500 4350 50 
$EndSheet
Text Label 5850 2200 0    50   ~ 0
ENC_CH1
Text Label 5850 2300 0    50   ~ 0
ENC_CH2
Wire Wire Line
	5850 2200 6500 2200
Wire Wire Line
	5850 2300 6500 2300
Text Label 4150 1800 0    50   ~ 0
BUZZ_EN2
Text Label 4150 1700 0    50   ~ 0
BUZZ_EN1
Wire Wire Line
	4050 1800 4750 1800
Wire Wire Line
	4050 1700 4750 1700
Text Label 8300 1600 0    50   ~ 0
BUZZ_DIN
Text Label 8300 1350 0    50   ~ 0
BUZZ_EN1
Text Label 8300 1450 0    50   ~ 0
BUZZ_EN2
Wire Wire Line
	8700 1350 8300 1350
Wire Wire Line
	8700 1450 8300 1450
Wire Wire Line
	8700 1600 8300 1600
$Comp
L Mechanical:MountingHole_Pad H1
U 1 1 5FA7B7EA
P 10050 5500
F 0 "H1" V 10050 5650 50  0000 L CNN
F 1 "MountingHole_Pad" V 10095 5650 50  0001 L CNN
F 2 "MountingHole:MountingHole_3.2mm_M3_Pad_Via" H 10050 5500 50  0001 C CNN
F 3 "~" H 10050 5500 50  0001 C CNN
	1    10050 5500
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole H5
U 1 1 5FA7D43F
P 10100 3000
F 0 "H5" H 10200 3000 50  0000 L CNN
F 1 "MountingHole" H 10200 2955 50  0001 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5_Pad" H 10100 3000 50  0001 C CNN
F 3 "~" H 10100 3000 50  0001 C CNN
	1    10100 3000
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole_Pad H2
U 1 1 5FA7DE48
P 10050 5700
F 0 "H2" V 10050 5850 50  0000 L CNN
F 1 "MountingHole_Pad" V 10095 5850 50  0001 L CNN
F 2 "MountingHole:MountingHole_3.2mm_M3_Pad_Via" H 10050 5700 50  0001 C CNN
F 3 "~" H 10050 5700 50  0001 C CNN
	1    10050 5700
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole_Pad H3
U 1 1 5FA7E086
P 10050 5900
F 0 "H3" V 10050 6050 50  0000 L CNN
F 1 "MountingHole_Pad" V 10095 6050 50  0001 L CNN
F 2 "MountingHole:MountingHole_3.2mm_M3_Pad_Via" H 10050 5900 50  0001 C CNN
F 3 "~" H 10050 5900 50  0001 C CNN
	1    10050 5900
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole_Pad H4
U 1 1 5FA7E48D
P 10050 6100
F 0 "H4" V 10050 6250 50  0000 L CNN
F 1 "MountingHole_Pad" V 10095 6250 50  0001 L CNN
F 2 "MountingHole:MountingHole_3.2mm_M3_Pad_Via" H 10050 6100 50  0001 C CNN
F 3 "~" H 10050 6100 50  0001 C CNN
	1    10050 6100
	0    1    1    0   
$EndComp
$Comp
L Mechanical:MountingHole H6
U 1 1 5FA7E88B
P 10100 3200
F 0 "H6" H 10200 3200 50  0000 L CNN
F 1 "MountingHole" H 10200 3155 50  0001 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5_Pad" H 10100 3200 50  0001 C CNN
F 3 "~" H 10100 3200 50  0001 C CNN
	1    10100 3200
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole H7
U 1 1 5FA7EB33
P 10100 3400
F 0 "H7" H 10200 3400 50  0000 L CNN
F 1 "MountingHole" H 10200 3355 50  0001 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5_Pad" H 10100 3400 50  0001 C CNN
F 3 "~" H 10100 3400 50  0001 C CNN
	1    10100 3400
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole H8
U 1 1 5FA7ED33
P 10100 3600
F 0 "H8" H 10200 3600 50  0000 L CNN
F 1 "MountingHole" H 10200 3555 50  0001 L CNN
F 2 "MountingHole:MountingHole_2.7mm_M2.5_Pad" H 10100 3600 50  0001 C CNN
F 3 "~" H 10100 3600 50  0001 C CNN
	1    10100 3600
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0101
U 1 1 5FAB16F8
P 9850 6200
F 0 "#PWR0101" H 9850 5950 50  0001 C CNN
F 1 "GND" H 9855 6027 50  0000 C CNN
F 2 "" H 9850 6200 50  0001 C CNN
F 3 "" H 9850 6200 50  0001 C CNN
	1    9850 6200
	1    0    0    -1  
$EndComp
Wire Wire Line
	9950 5500 9850 5500
Wire Wire Line
	9850 5500 9850 5700
Wire Wire Line
	9950 5700 9850 5700
Connection ~ 9850 5700
Wire Wire Line
	9850 5700 9850 5900
Wire Wire Line
	9950 5900 9850 5900
Connection ~ 9850 5900
Wire Wire Line
	9850 5900 9850 6100
Wire Wire Line
	9950 6100 9850 6100
Connection ~ 9850 6100
Wire Wire Line
	9850 6100 9850 6200
Text Notes 9650 5400 0    50   ~ 0
Enclosure Mounting
Wire Notes Line
	8900 2500 8900 5200
Wire Notes Line
	8900 5200 10450 5200
Wire Notes Line
	10450 5200 10450 2500
Wire Notes Line
	10450 2500 8900 2500
Wire Notes Line
	9600 5300 9600 6450
Wire Notes Line
	9600 6450 10450 6450
Wire Notes Line
	10450 6450 10450 5300
Wire Notes Line
	10450 5300 9600 5300
Text Notes 600  5650 0    50   ~ 0
Debug Connector
Wire Notes Line
	550  5500 550  7450
Wire Notes Line
	550  7450 3200 7450
Wire Notes Line
	3200 7450 3200 5500
Wire Notes Line
	3200 5500 550  5500
Wire Notes Line
	8250 550  8250 2400
Wire Notes Line
	8250 2400 10450 2400
Wire Notes Line
	10450 2400 10450 550 
Wire Notes Line
	10450 550  8250 550 
Wire Wire Line
	4250 6250 4650 6250
Wire Wire Line
	4250 6350 4650 6350
Wire Notes Line
	3300 5500 3300 6900
Wire Notes Line
	3300 6900 4700 6900
Wire Notes Line
	4700 6900 4700 5500
Wire Notes Line
	4700 5500 3300 5500
Wire Notes Line
	550  550  5500 550 
Wire Notes Line
	5500 550  5500 5400
Wire Notes Line
	5500 5400 550  5400
Wire Notes Line
	550  5400 550  550 
Text Notes 8950 2650 0    50   ~ 0
Display Connection & Mounting
Text Notes 8300 700  0    50   ~ 0
Piezo Buzzer
Text Notes 650  700  0    50   ~ 0
Microcontroller
Text Notes 3350 5650 0    50   ~ 0
EEPROM
NoConn ~ 4050 3600
$EndSCHEMATC
