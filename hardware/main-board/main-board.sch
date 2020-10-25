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
L MCU_ST_STM32F4:STM32F411RETx U?
U 1 1 5F8A24AA
P 3600 3250
F 0 "U?" H 3050 4900 50  0000 C CNN
F 1 "STM32F411RETx" H 4150 1500 50  0000 C CNN
F 2 "Package_QFP:LQFP-64_10x10mm_P0.5mm" H 3000 1550 50  0001 R CNN
F 3 "http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00115249.pdf" H 3600 3250 50  0001 C CNN
	1    3600 3250
	1    0    0    -1  
$EndComp
$Comp
L Device:Speaker_Crystal LS?
U 1 1 5F8D143F
P 9600 1050
F 0 "LS?" H 9775 1046 50  0000 L CNN
F 1 "Speaker_Crystal" H 9775 955 50  0000 L CNN
F 2 "" H 9565 1000 50  0001 C CNN
F 3 "~" H 9565 1000 50  0001 C CNN
	1    9600 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:R R?
U 1 1 5F8D2514
P 9100 1100
F 0 "R?" H 9170 1146 50  0000 L CNN
F 1 "1K" H 9170 1055 50  0000 L CNN
F 2 "" V 9030 1100 50  0001 C CNN
F 3 "~" H 9100 1100 50  0001 C CNN
	1    9100 1100
	1    0    0    -1  
$EndComp
$Comp
L Transistor_BJT:2SC1815 Q?
U 1 1 5F8D3A87
P 9000 1450
F 0 "Q?" H 9190 1496 50  0000 L CNN
F 1 "2SC1815" H 9190 1405 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 9200 1375 50  0001 L CIN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Toshiba%20PDFs/2SC1815.pdf" H 9000 1450 50  0001 L CNN
	1    9000 1450
	1    0    0    -1  
$EndComp
Wire Wire Line
	9100 1250 9400 1250
Wire Wire Line
	9400 1250 9400 1150
Connection ~ 9100 1250
Wire Wire Line
	9400 1050 9400 950 
Wire Wire Line
	9400 950  9100 950 
$Comp
L power:+5V #PWR?
U 1 1 5F8D6021
P 9100 900
F 0 "#PWR?" H 9100 750 50  0001 C CNN
F 1 "+5V" H 9115 1073 50  0000 C CNN
F 2 "" H 9100 900 50  0001 C CNN
F 3 "" H 9100 900 50  0001 C CNN
	1    9100 900 
	1    0    0    -1  
$EndComp
Wire Wire Line
	9100 900  9100 950 
Connection ~ 9100 950 
$Comp
L power:GND #PWR?
U 1 1 5F8D7106
P 9100 1700
F 0 "#PWR?" H 9100 1450 50  0001 C CNN
F 1 "GND" H 9105 1527 50  0000 C CNN
F 2 "" H 9100 1700 50  0001 C CNN
F 3 "" H 9100 1700 50  0001 C CNN
	1    9100 1700
	1    0    0    -1  
$EndComp
Wire Wire Line
	9100 1650 9100 1700
Text GLabel 8700 1450 0    50   Input ~ 0
BUZZER
Wire Wire Line
	8800 1450 8700 1450
$Sheet
S 6500 700  1450 750 
U 5F8B674D
F0 "Power Supply and Control" 50
F1 "main-board-power.sch" 50
$EndSheet
$Sheet
S 6500 1750 1450 750 
U 5F8B69B5
F0 "User Input" 50
F1 "main-board-input.sch" 50
$EndSheet
$Comp
L power:GND #PWR?
U 1 1 5F8B9F00
P 9050 6150
F 0 "#PWR?" H 9050 5900 50  0001 C CNN
F 1 "GND" H 9055 5977 50  0000 C CNN
F 2 "" H 9050 6150 50  0001 C CNN
F 3 "" H 9050 6150 50  0001 C CNN
	1    9050 6150
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5F8BA7DD
P 9000 4050
F 0 "#PWR?" H 9000 3900 50  0001 C CNN
F 1 "+3.3V" H 9015 4223 50  0000 C CNN
F 2 "" H 9000 4050 50  0001 C CNN
F 3 "" H 9000 4050 50  0001 C CNN
	1    9000 4050
	1    0    0    -1  
$EndComp
Wire Wire Line
	9150 4250 9000 4250
Wire Wire Line
	9000 4250 9000 4050
NoConn ~ 9150 4350
NoConn ~ 9150 4950
NoConn ~ 9150 5550
NoConn ~ 9150 5850
Wire Wire Line
	9150 4150 9050 4150
Wire Wire Line
	9050 4150 9050 4550
Wire Wire Line
	9150 4550 9050 4550
Connection ~ 9050 4550
Wire Wire Line
	9050 4550 9050 4650
Wire Wire Line
	9150 4650 9050 4650
Connection ~ 9050 4650
Wire Wire Line
	9150 5050 9050 5050
Connection ~ 9050 5050
Wire Wire Line
	9050 5050 9050 5150
Wire Wire Line
	9150 5150 9050 5150
Connection ~ 9050 5150
Wire Wire Line
	9050 5150 9050 5250
Wire Wire Line
	9150 5250 9050 5250
Connection ~ 9050 5250
Wire Wire Line
	9050 5250 9050 5350
Wire Wire Line
	9150 5350 9050 5350
Connection ~ 9050 5350
Wire Wire Line
	9050 5350 9050 5450
Wire Wire Line
	9150 5450 9050 5450
Connection ~ 9050 5450
Wire Wire Line
	9050 5450 9050 5950
Wire Wire Line
	9150 5950 9050 5950
Connection ~ 9050 5950
Wire Wire Line
	9050 5950 9050 6050
Wire Wire Line
	9150 6050 9050 6050
Connection ~ 9050 6050
Wire Wire Line
	9050 6050 9050 6150
Wire Wire Line
	9150 4450 8900 4450
Wire Wire Line
	9150 4750 8900 4750
Wire Wire Line
	9150 5650 8900 5650
Wire Wire Line
	9150 5750 8900 5750
Text Label 8900 4450 2    50   ~ 0
DISP_D~C
Text Label 8900 4750 2    50   ~ 0
DISP_SPI_SCK
Text Label 8900 4850 2    50   ~ 0
DISP_SPI_MOSI
Text Label 8900 5650 2    50   ~ 0
DISP_~RES
Text Label 8900 5750 2    50   ~ 0
DISP_~CS
$Sheet
S 6500 2750 1450 800 
U 5FA19AFF
F0 "Front External Connections" 50
F1 "main-board-front.sch" 50
$EndSheet
$Comp
L Project:Conn_ST_STDC14 J?
U 1 1 5FA6AC8D
P 9100 2900
F 0 "J?" H 8657 2946 50  0000 R CNN
F 1 "STDC14" H 8657 2855 50  0000 R CNN
F 2 "lib_fp:Samtec_FTSH-107-01-L-DV-K-A_2x07_P1.27mm" H 9100 1800 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/user_manual/group1/99/49/91/b6/b2/3a/46/e5/DM00526767/files/DM00526767.pdf/jcr:content/translations/en.DM00526767.pdf" V 8750 1650 50  0001 C CNN
	1    9100 2900
	1    0    0    -1  
$EndComp
Wire Wire Line
	9050 4650 9050 5050
Wire Wire Line
	9150 4850 8900 4850
$Comp
L Connector_Generic:Conn_01x20 J?
U 1 1 5F8B8832
P 9350 5050
F 0 "J?" H 9300 6050 50  0000 L CNN
F 1 "NHD OLED Connector" H 9150 3950 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x20_P2.54mm_Vertical" H 9350 5050 50  0001 C CNN
F 3 "~" H 9350 5050 50  0001 C CNN
	1    9350 5050
	1    0    0    -1  
$EndComp
Text Notes 9250 1850 0    50   Italic 0
Find a suitable SMD transistor\nto drive the speaker, or some\nother driver circuit.
$Comp
L Device:Crystal_GND24 Y?
U 1 1 5F94F8E6
P 2000 3200
F 0 "Y?" H 2050 3400 50  0000 L CNN
F 1 "16MHz" H 2050 3000 50  0000 L CNN
F 2 "lib_fp:Crystal_SMD_Kyocera_CX3225GB" H 2000 3200 50  0001 C CNN
F 3 "http://prdct-search.kyocera.co.jp/crystal-ic/catalog/en/cx3225gb_e.pdf" H 2000 3200 50  0001 C CNN
F 4 "CRYSTAL 16.0000MHZ 8PF SMD" H 2000 3200 50  0001 C CNN "Description"
F 5 "Kyocera International Inc." H 2000 3200 50  0001 C CNN "Manufacturer"
F 6 "CX3225GB16000D0HPQCC" H 2000 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2000 3200 50  0001 C CNN "Supplier"
F 8 "1253-1181-1-ND" H 2000 3200 50  0001 C CNN "Supplier PN"
	1    2000 3200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F9506A3
P 1750 3550
F 0 "C?" H 1842 3596 50  0000 L CNN
F 1 "8pF" H 1842 3505 50  0000 L CNN
F 2 "" H 1750 3550 50  0001 C CNN
F 3 "~" H 1750 3550 50  0001 C CNN
	1    1750 3550
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F950FD6
P 2250 3550
F 0 "C?" H 2342 3596 50  0000 L CNN
F 1 "8pF" H 2342 3505 50  0000 L CNN
F 2 "" H 2250 3550 50  0001 C CNN
F 3 "~" H 2250 3550 50  0001 C CNN
	1    2250 3550
	1    0    0    -1  
$EndComp
Wire Wire Line
	1850 3200 1750 3200
Wire Wire Line
	1750 3200 1750 3450
Wire Wire Line
	2150 3200 2250 3200
Wire Wire Line
	2250 3200 2250 3450
Wire Wire Line
	2900 2950 2250 2950
Wire Wire Line
	2250 2950 2250 3200
Connection ~ 2250 3200
Wire Wire Line
	2900 2850 1750 2850
Wire Wire Line
	1750 2850 1750 3200
Connection ~ 1750 3200
$Comp
L power:GND #PWR?
U 1 1 5F95AD21
P 1750 3650
F 0 "#PWR?" H 1750 3400 50  0001 C CNN
F 1 "GND" H 1755 3477 50  0000 C CNN
F 2 "" H 1750 3650 50  0001 C CNN
F 3 "" H 1750 3650 50  0001 C CNN
	1    1750 3650
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5F95B4D9
P 2250 3650
F 0 "#PWR?" H 2250 3400 50  0001 C CNN
F 1 "GND" H 2255 3477 50  0000 C CNN
F 2 "" H 2250 3650 50  0001 C CNN
F 3 "" H 2250 3650 50  0001 C CNN
	1    2250 3650
	1    0    0    -1  
$EndComp
NoConn ~ 2000 3400
NoConn ~ 2000 3000
$Comp
L Project:M24M01-RMN U?
U 1 1 5F950792
P 7200 4550
F 0 "U?" H 7200 5031 50  0000 C CNN
F 1 "M24M01-RMN" H 7200 4940 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 7200 4900 50  0001 C CNN
F 3 "https://www.st.com/content/ccc/resource/technical/document/datasheet/group0/cb/91/ba/7d/0b/c1/4d/f6/CD00147128/files/CD00147128.pdf/jcr:content/translations/en.CD00147128.pdf" H 7250 4050 50  0001 C CNN
	1    7200 4550
	1    0    0    -1  
$EndComp
$Comp
L Device:L_Small L?
U 1 1 5F951860
P 4650 900
F 0 "L?" V 4469 900 50  0000 C CNN
F 1 "600 Ohm" V 4560 900 50  0000 C CNN
F 2 "Inductor_SMD:L_0603_1608Metric" H 4650 900 50  0001 C CNN
F 3 "https://www.yuden.co.jp/productdata/catalog/mlci07_e.pdf" H 4650 900 50  0001 C CNN
F 4 "FERRITE BEAD 600 OHM 0603 1LN" H 4650 900 50  0001 C CNN "Description"
F 5 "Taiyo Yuden" H 4650 900 50  0001 C CNN "Manufacturer"
F 6 "BK1608HS601-T" H 4650 900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4650 900 50  0001 C CNN "Supplier"
F 8 "587-1874-1-ND" H 4650 900 50  0001 C CNN "Supplier PN"
	1    4650 900 
	0    1    1    0   
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5F96416A
P 4950 900
F 0 "#PWR?" H 4950 750 50  0001 C CNN
F 1 "+3.3V" H 4965 1073 50  0000 C CNN
F 2 "" H 4950 900 50  0001 C CNN
F 3 "" H 4950 900 50  0001 C CNN
	1    4950 900 
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F9656F7
P 4450 1250
F 0 "C?" H 4542 1296 50  0000 L CNN
F 1 "0.1uF" H 4542 1205 50  0000 L CNN
F 2 "" H 4450 1250 50  0001 C CNN
F 3 "~" H 4450 1250 50  0001 C CNN
	1    4450 1250
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F965D4A
P 4850 1250
F 0 "C?" H 4942 1296 50  0000 L CNN
F 1 "1uF" H 4942 1205 50  0000 L CNN
F 2 "" H 4850 1250 50  0001 C CNN
F 3 "~" H 4850 1250 50  0001 C CNN
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
L power:GND #PWR?
U 1 1 5F969C61
P 4450 1400
F 0 "#PWR?" H 4450 1150 50  0001 C CNN
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
L Device:C_Small C?
U 1 1 5F97FD14
P 3000 1200
F 0 "C?" H 3092 1246 50  0000 L CNN
F 1 "0.1uF" H 3092 1155 50  0000 L CNN
F 2 "" H 3000 1200 50  0001 C CNN
F 3 "~" H 3000 1200 50  0001 C CNN
	1    3000 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F980A35
P 2600 1200
F 0 "C?" H 2692 1246 50  0000 L CNN
F 1 "0.1uF" H 2692 1155 50  0000 L CNN
F 2 "" H 2600 1200 50  0001 C CNN
F 3 "~" H 2600 1200 50  0001 C CNN
	1    2600 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F980D6F
P 2200 1200
F 0 "C?" H 2292 1246 50  0000 L CNN
F 1 "0.1uF" H 2292 1155 50  0000 L CNN
F 2 "" H 2200 1200 50  0001 C CNN
F 3 "~" H 2200 1200 50  0001 C CNN
	1    2200 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F9811DA
P 1800 1200
F 0 "C?" H 1892 1246 50  0000 L CNN
F 1 "0.1uF" H 1892 1155 50  0000 L CNN
F 2 "" H 1800 1200 50  0001 C CNN
F 3 "~" H 1800 1200 50  0001 C CNN
	1    1800 1200
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C?
U 1 1 5F981EC3
P 1400 1200
F 0 "C?" H 1492 1246 50  0000 L CNN
F 1 "0.1uF" H 1492 1155 50  0000 L CNN
F 2 "" H 1400 1200 50  0001 C CNN
F 3 "~" H 1400 1200 50  0001 C CNN
	1    1400 1200
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5F98252A
P 1000 1350
F 0 "#PWR?" H 1000 1100 50  0001 C CNN
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
L power:+3.3V #PWR?
U 1 1 5F988BE8
P 1000 1050
F 0 "#PWR?" H 1000 900 50  0001 C CNN
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
L Device:C_Small C?
U 1 1 5F9A4479
P 1000 1200
F 0 "C?" H 1092 1246 50  0000 L CNN
F 1 "10 uF" H 1092 1155 50  0000 L CNN
F 2 "" H 1000 1200 50  0001 C CNN
F 3 "~" H 1000 1200 50  0001 C CNN
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
L Device:C_Small C?
U 1 1 5F9AEFEC
P 2650 2300
F 0 "C?" H 2742 2346 50  0000 L CNN
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
L power:GND #PWR?
U 1 1 5F9AF7F7
P 2650 2400
F 0 "#PWR?" H 2650 2150 50  0001 C CNN
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
L Device:R_Small R?
U 1 1 5F9B3310
P 2350 2150
F 0 "R?" H 2409 2196 50  0000 L CNN
F 1 "10K" H 2409 2105 50  0000 L CNN
F 2 "" H 2350 2150 50  0001 C CNN
F 3 "~" H 2350 2150 50  0001 C CNN
	1    2350 2150
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5F9B41EF
P 2350 2250
F 0 "#PWR?" H 2350 2000 50  0001 C CNN
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
L power:GND #PWR?
U 1 1 5F9BB459
P 3800 5150
F 0 "#PWR?" H 3800 4900 50  0001 C CNN
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
L Device:C_Small C?
U 1 1 5F9CAEBD
P 1950 1950
F 0 "C?" H 2042 1996 50  0000 L CNN
F 1 "0.1uF" H 2042 1905 50  0000 L CNN
F 2 "" H 1950 1950 50  0001 C CNN
F 3 "~" H 1950 1950 50  0001 C CNN
	1    1950 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2900 1750 1950 1750
Wire Wire Line
	1950 1750 1950 1850
$Comp
L power:GND #PWR?
U 1 1 5F9CF1A9
P 1950 2050
F 0 "#PWR?" H 1950 1800 50  0001 C CNN
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
Text Label 9900 2400 2    50   ~ 0
RESET
Wire Wire Line
	9600 2400 9900 2400
$EndSCHEMATC
