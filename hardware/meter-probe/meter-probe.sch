EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 1 1
Title "Printalyzer - Meter Probe"
Date "2020-10-28"
Rev "A"
Comp "LogicProbe.org"
Comment1 "Derek Konigsberg"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Project:RT9193 U1
U 1 1 5F8A0F5B
P 3700 4200
F 0 "U1" H 3700 4542 50  0000 C CNN
F 1 "RT9193" H 3700 4451 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23-5" H 3700 4500 50  0001 C CIN
F 3 "https://www.richtek.com/assets/product_file/RT9193/DS9193-17.pdf" H 3700 4200 50  0001 C CNN
F 4 "IC REG LINEAR 3.3V 300MA SOT23-5" H 3700 4200 50  0001 C CNN "Description"
F 5 "Richtek USA Inc." H 3700 4200 50  0001 C CNN "Manufacturer"
F 6 "RT9193-33GB" H 3700 4200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3700 4200 50  0001 C CNN "Supplier"
F 8 "1028-1014-1-ND" H 3700 4200 50  0001 C CNN "Supplier PN"
	1    3700 4200
	1    0    0    -1  
$EndComp
$Comp
L Project:TCS3472 U2
U 1 1 5F8A152C
P 1700 1850
F 0 "U2" H 1256 1771 50  0000 R CNN
F 1 "TCS3472" H 1256 1680 50  0000 R CNN
F 2 "lib_fp:AMS_TCS3472_DFN-6" H 1700 1350 50  0001 C CNN
F 3 "https://ams.com/documents/20143/36005/TCS3472_DS000390_3-00.pdf" H 2450 2150 50  0001 C CNN
F 4 "IC COLOR CONV LIGHT-DGTL 6-DFN" H 1700 1850 50  0001 C CNN "Description"
F 5 "ams" H 1700 1850 50  0001 C CNN "Manufacturer"
F 6 "TCS34725FN" H 1700 1850 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1700 1850 50  0001 C CNN "Supplier"
F 8 "TCS34725FNCT-ND" H 1700 1850 50  0001 C CNN "Supplier PN"
	1    1700 1850
	1    0    0    -1  
$EndComp
$Comp
L Marquadt_Switch:SW_Key_6425_0101 SW1
U 1 1 5F8A2A8F
P 1300 4250
F 0 "SW1" H 1300 4535 50  0000 C CNN
F 1 "Sensor Button" H 1300 4444 50  0000 C CNN
F 2 "lib_fp:SW_Key_6425_0101" H 1300 4450 50  0001 C CNN
F 3 "https://www.marquardt-switches.com/PLM-uploads/kzeichnung/64250101_00_K.pdf" H 1300 4450 50  0001 C CNN
F 4 "Switch 6425.0101 with button 827.100.011" H 1300 4250 50  0001 C CNN "Description"
F 5 "Marquardt Switches" H 1300 4250 50  0001 C CNN "Manufacturer"
F 6 "6425.0101" H 1300 4250 50  0001 C CNN "Manufacturer PN"
F 7 "Mouser" H 1300 4250 50  0001 C CNN "Supplier"
F 8 "979-6425.0101" H 1300 4250 50  0001 C CNN "Supplier PN"
	1    1300 4250
	1    0    0    -1  
$EndComp
$Comp
L Interface_Expansion:P82B96 U3
U 1 1 5F8A31B7
P 3900 1950
F 0 "U3" H 3900 2567 50  0000 C CNN
F 1 "P82B96" H 3900 2476 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 3900 1950 50  0001 C CNN
F 3 "http://www.ti.com/general/docs/suppproductinfo.tsp?distId=10&gotoUrl=http%3A%2F%2Fwww.ti.com%2Flit%2Fgpn%2Fp82b96" H 3900 1950 50  0001 C CNN
F 4 "IC REDRIVER I2C 1CH 400KHZ 8SOIC" H 3900 1950 50  0001 C CNN "Description"
F 5 "Texas Instruments" H 3900 1950 50  0001 C CNN "Manufacturer"
F 6 "P82B96DR" H 3900 1950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3900 1950 50  0001 C CNN "Supplier"
F 8 "296-20942-1-ND" H 3900 1950 50  0001 C CNN "Supplier PN"
	1    3900 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	4100 4200 4200 4200
Wire Wire Line
	4200 4200 4200 4300
Wire Wire Line
	3300 4100 3000 4100
Wire Wire Line
	3300 4200 3300 4100
Connection ~ 3300 4100
Wire Wire Line
	4100 4100 4600 4100
$Comp
L power:GND #PWR0101
U 1 1 5F8BE462
P 4200 4500
F 0 "#PWR0101" H 4200 4250 50  0001 C CNN
F 1 "GND" H 4205 4327 50  0000 C CNN
F 2 "" H 4200 4500 50  0001 C CNN
F 3 "" H 4200 4500 50  0001 C CNN
	1    4200 4500
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0102
U 1 1 5F8BEDF3
P 4600 4300
F 0 "#PWR0102" H 4600 4050 50  0001 C CNN
F 1 "GND" H 4605 4127 50  0000 C CNN
F 2 "" H 4600 4300 50  0001 C CNN
F 3 "" H 4600 4300 50  0001 C CNN
	1    4600 4300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0103
U 1 1 5F8BF4E8
P 3000 4300
F 0 "#PWR0103" H 3000 4050 50  0001 C CNN
F 1 "GND" H 3005 4127 50  0000 C CNN
F 2 "" H 3000 4300 50  0001 C CNN
F 3 "" H 3000 4300 50  0001 C CNN
	1    3000 4300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0104
U 1 1 5F8C1BAD
P 3700 4500
F 0 "#PWR0104" H 3700 4250 50  0001 C CNN
F 1 "GND" H 3705 4327 50  0000 C CNN
F 2 "" H 3700 4500 50  0001 C CNN
F 3 "" H 3700 4500 50  0001 C CNN
	1    3700 4500
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR0105
U 1 1 5F8C2B7F
P 4600 4000
F 0 "#PWR0105" H 4600 3850 50  0001 C CNN
F 1 "+3.3V" H 4615 4173 50  0000 C CNN
F 2 "" H 4600 4000 50  0001 C CNN
F 3 "" H 4600 4000 50  0001 C CNN
	1    4600 4000
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0106
U 1 1 5F8C33B0
P 3300 4000
F 0 "#PWR0106" H 3300 3850 50  0001 C CNN
F 1 "+5V" H 3315 4173 50  0000 C CNN
F 2 "" H 3300 4000 50  0001 C CNN
F 3 "" H 3300 4000 50  0001 C CNN
	1    3300 4000
	1    0    0    -1  
$EndComp
Wire Wire Line
	4600 4000 4600 4100
Wire Wire Line
	3300 4000 3300 4100
Wire Wire Line
	4500 2050 4500 2250
Wire Wire Line
	4500 1650 4500 1850
$Comp
L power:+5V #PWR0107
U 1 1 5F8C56D1
P 3300 1350
F 0 "#PWR0107" H 3300 1200 50  0001 C CNN
F 1 "+5V" H 3315 1523 50  0000 C CNN
F 2 "" H 3300 1350 50  0001 C CNN
F 3 "" H 3300 1350 50  0001 C CNN
	1    3300 1350
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0108
U 1 1 5F8C593E
P 3200 2350
F 0 "#PWR0108" H 3200 2100 50  0001 C CNN
F 1 "GND" H 3205 2177 50  0000 C CNN
F 2 "" H 3200 2350 50  0001 C CNN
F 3 "" H 3200 2350 50  0001 C CNN
	1    3200 2350
	1    0    0    -1  
$EndComp
Wire Wire Line
	3300 2250 3200 2250
Wire Wire Line
	3200 2250 3200 2350
$Comp
L power:GND #PWR0109
U 1 1 5F8C820C
P 1500 1400
F 0 "#PWR0109" H 1500 1150 50  0001 C CNN
F 1 "GND" H 1505 1227 50  0000 C CNN
F 2 "" H 1500 1400 50  0001 C CNN
F 3 "" H 1500 1400 50  0001 C CNN
	1    1500 1400
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR0110
U 1 1 5F8C87E4
P 1800 1250
F 0 "#PWR0110" H 1800 1100 50  0001 C CNN
F 1 "+3.3V" H 1815 1423 50  0000 C CNN
F 2 "" H 1800 1250 50  0001 C CNN
F 3 "" H 1800 1250 50  0001 C CNN
	1    1800 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0111
U 1 1 5F8C8F30
P 1800 2500
F 0 "#PWR0111" H 1800 2250 50  0001 C CNN
F 1 "GND" H 1805 2327 50  0000 C CNN
F 2 "" H 1800 2500 50  0001 C CNN
F 3 "" H 1800 2500 50  0001 C CNN
	1    1800 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	1800 2400 1800 2500
$Comp
L Connector_Generic:Conn_01x06 J1
U 1 1 5F8CB1BB
P 6550 1550
F 0 "J1" H 6500 1850 50  0000 L CNN
F 1 "Main Board" H 6450 1150 50  0000 L CNN
F 2 "lib_fp:SolderWire-0.1sqmm_1x06_Cable" H 6550 1550 50  0001 C CNN
F 3 "~" H 6550 1550 50  0001 C CNN
F 4 "WIRE-TO-BOARD" H 6550 1550 50  0001 C CNN "Description"
	1    6550 1550
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0114
U 1 1 5F93106A
P 3000 1500
F 0 "#PWR0114" H 3000 1250 50  0001 C CNN
F 1 "GND" H 3005 1327 50  0000 C CNN
F 2 "" H 3000 1500 50  0001 C CNN
F 3 "" H 3000 1500 50  0001 C CNN
	1    3000 1500
	1    0    0    -1  
$EndComp
Wire Wire Line
	2100 1850 2450 1850
Wire Wire Line
	2100 1950 2200 1950
Wire Wire Line
	3300 1950 3300 2050
Wire Wire Line
	1800 1250 1800 1400
Connection ~ 1800 1400
Wire Wire Line
	1800 1400 1800 1550
Wire Wire Line
	2450 1700 2450 1850
Wire Wire Line
	2450 1850 3300 1850
Wire Wire Line
	2200 1700 2200 1950
Wire Wire Line
	2200 1950 3300 1950
Wire Wire Line
	2200 1500 2200 1400
Wire Wire Line
	2200 1400 1800 1400
Wire Wire Line
	2450 1500 2450 1400
Wire Wire Line
	2450 1400 2200 1400
Connection ~ 2200 1400
$Comp
L power:GND #PWR0115
U 1 1 5F9461F9
P 1000 4350
F 0 "#PWR0115" H 1000 4100 50  0001 C CNN
F 1 "GND" H 1005 4177 50  0000 C CNN
F 2 "" H 1000 4350 50  0001 C CNN
F 3 "" H 1000 4350 50  0001 C CNN
	1    1000 4350
	1    0    0    -1  
$EndComp
Wire Wire Line
	1100 4250 1000 4250
Wire Wire Line
	1000 4250 1000 4350
$Comp
L power:+5V #PWR0116
U 1 1 5F947727
P 1700 3950
F 0 "#PWR0116" H 1700 3800 50  0001 C CNN
F 1 "+5V" H 1715 4123 50  0000 C CNN
F 2 "" H 1700 3950 50  0001 C CNN
F 3 "" H 1700 3950 50  0001 C CNN
	1    1700 3950
	1    0    0    -1  
$EndComp
$Comp
L Transistor_FET:BSS138 Q1
U 1 1 5F8E011B
P 3850 3000
F 0 "Q1" V 4099 3000 50  0000 C CNN
F 1 "BSS138" V 4190 3000 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 4050 2925 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3850 3000 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3850 3000 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3850 3000 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3850 3000 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3850 3000 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3850 3000 50  0001 C CNN "Supplier PN"
	1    3850 3000
	0    1    1    0   
$EndComp
Wire Wire Line
	6350 1550 6250 1550
Wire Wire Line
	6250 1550 6250 1950
Wire Wire Line
	6150 1650 6350 1650
Text Label 6000 1350 2    50   ~ 0
SDA
Text Label 6000 1450 2    50   ~ 0
~INT
Text Label 6000 1750 2    50   ~ 0
SCL
Text Label 6000 1850 2    50   ~ 0
~BUTTON
Wire Wire Line
	6000 1350 6350 1350
Wire Wire Line
	6000 1450 6350 1450
Wire Wire Line
	6000 1750 6350 1750
Wire Wire Line
	6000 1850 6350 1850
$Comp
L Device:R_Small R4
U 1 1 5F8EF38B
P 3500 2900
F 0 "R4" H 3559 2946 50  0000 L CNN
F 1 "10K" H 3559 2855 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 3500 2900 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3500 2900 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3500 2900 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3500 2900 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3500 2900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3500 2900 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3500 2900 50  0001 C CNN "Supplier PN"
	1    3500 2900
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R5
U 1 1 5F8F06C6
P 4200 2900
F 0 "R5" H 4259 2946 50  0000 L CNN
F 1 "10K" H 4259 2855 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 4200 2900 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 4200 2900 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 4200 2900 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 4200 2900 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 4200 2900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4200 2900 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 4200 2900 50  0001 C CNN "Supplier PN"
	1    4200 2900
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR0117
U 1 1 5F8F145D
P 3500 2750
F 0 "#PWR0117" H 3500 2600 50  0001 C CNN
F 1 "+3.3V" H 3515 2923 50  0000 C CNN
F 2 "" H 3500 2750 50  0001 C CNN
F 3 "" H 3500 2750 50  0001 C CNN
	1    3500 2750
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0118
U 1 1 5F8F1E4D
P 4200 2750
F 0 "#PWR0118" H 4200 2600 50  0001 C CNN
F 1 "+5V" H 4215 2923 50  0000 C CNN
F 2 "" H 4200 2750 50  0001 C CNN
F 3 "" H 4200 2750 50  0001 C CNN
	1    4200 2750
	1    0    0    -1  
$EndComp
Wire Wire Line
	4050 3100 4200 3100
Wire Wire Line
	4200 3100 4200 3000
Wire Wire Line
	3650 3100 3500 3100
Wire Wire Line
	3500 3100 3500 3000
$Comp
L Device:R_Small R2
U 1 1 5F8FA201
P 2200 1600
F 0 "R2" H 2259 1646 50  0000 L CNN
F 1 "2K" H 2259 1555 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 2200 1600 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2200 1600 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 2200 1600 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2200 1600 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 2200 1600 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2200 1600 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 2200 1600 50  0001 C CNN "Supplier PN"
	1    2200 1600
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R3
U 1 1 5F8FA6F9
P 2450 1600
F 0 "R3" H 2509 1646 50  0000 L CNN
F 1 "2K" H 2509 1555 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 2450 1600 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2450 1600 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 2450 1600 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2450 1600 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 2450 1600 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2450 1600 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 2450 1600 50  0001 C CNN "Supplier PN"
	1    2450 1600
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C4
U 1 1 5F8FD46B
P 1600 1400
F 0 "C4" V 1371 1400 50  0000 C CNN
F 1 "0.1uF" V 1462 1400 50  0000 C CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1600 1400 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 1600 1400 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 1600 1400 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 1600 1400 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 1600 1400 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1600 1400 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 1600 1400 50  0001 C CNN "Supplier PN"
	1    1600 1400
	0    1    1    0   
$EndComp
Wire Wire Line
	3300 1350 3300 1500
$Comp
L Device:C_Small C5
U 1 1 5F8FDA8A
P 3100 1500
F 0 "C5" V 2871 1500 50  0000 C CNN
F 1 "0.1uF" V 2962 1500 50  0000 C CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 3100 1500 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 3100 1500 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 3100 1500 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 3100 1500 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 3100 1500 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3100 1500 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 3100 1500 50  0001 C CNN "Supplier PN"
	1    3100 1500
	0    1    1    0   
$EndComp
Wire Wire Line
	1700 1400 1800 1400
Wire Wire Line
	3200 1500 3300 1500
Connection ~ 3300 1500
Wire Wire Line
	3300 1500 3300 1650
Wire Wire Line
	2100 2050 2600 2050
Wire Wire Line
	2600 2050 2600 3100
Wire Wire Line
	2600 3100 3500 3100
Connection ~ 3500 3100
Text Label 4750 1850 0    50   ~ 0
SDA
Text Label 4750 2250 0    50   ~ 0
SCL
Wire Wire Line
	4500 1850 4750 1850
Connection ~ 4500 1850
Wire Wire Line
	4500 2250 4750 2250
Connection ~ 4500 2250
Text Label 4750 3100 0    50   ~ 0
~INT
Wire Wire Line
	4200 3100 4750 3100
Connection ~ 4200 3100
$Comp
L Device:R_Small R1
U 1 1 5F91731C
P 1700 4050
F 0 "R1" H 1759 4096 50  0000 L CNN
F 1 "10K" H 1759 4005 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" H 1700 4050 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1700 4050 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 1700 4050 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1700 4050 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 1700 4050 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1700 4050 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 1700 4050 50  0001 C CNN "Supplier PN"
	1    1700 4050
	1    0    0    -1  
$EndComp
Wire Wire Line
	1500 4250 1700 4250
Wire Wire Line
	1700 4150 1700 4250
Connection ~ 1700 4250
Wire Wire Line
	1700 4250 1900 4250
Text Label 1900 4250 0    50   ~ 0
~BUTTON
$Comp
L Device:C_Small C1
U 1 1 5F919F34
P 3000 4200
F 0 "C1" H 2908 4154 50  0000 R CNN
F 1 "1uF" H 2908 4245 50  0000 R CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 3000 4200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL10B105KA8NNNC_char.pdf" H 3000 4200 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 3000 4200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 3000 4200 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 3000 4200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3000 4200 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 3000 4200 50  0001 C CNN "Supplier PN"
	1    3000 4200
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C2
U 1 1 5F91A815
P 4600 4200
F 0 "C2" H 4508 4154 50  0000 R CNN
F 1 "1uF" H 4508 4245 50  0000 R CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4600 4200 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL10B105KA8NNNC_char.pdf" H 4600 4200 50  0001 C CNN
F 4 "CAP CER 1UF 25V X7R 0603" H 4600 4200 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 4600 4200 50  0001 C CNN "Manufacturer"
F 6 "CL10B105KA8NNNC" H 4600 4200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4600 4200 50  0001 C CNN "Supplier"
F 8 "1276-1184-1-ND" H 4600 4200 50  0001 C CNN "Supplier PN"
	1    4600 4200
	-1   0    0    1   
$EndComp
Connection ~ 4600 4100
$Comp
L Device:C_Small C3
U 1 1 5F91BD98
P 4200 4400
F 0 "C3" H 4108 4354 50  0000 R CNN
F 1 "22nF" H 4108 4445 50  0000 R CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 4200 4400 50  0001 C CNN
F 3 "https://product.tdk.com/info/en/documents/chara_sheet/C2012C0G1H223J125AA.pdf" H 4200 4400 50  0001 C CNN
F 4 "CAP CER 0.022UF 50V C0G 0805" H 4200 4400 50  0001 C CNN "Description"
F 5 "TDK Corporation" H 4200 4400 50  0001 C CNN "Manufacturer"
F 6 "C2012C0G1H223J125AA" H 4200 4400 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4200 4400 50  0001 C CNN "Supplier"
F 8 "445-7522-1-ND" H 4200 4400 50  0001 C CNN "Supplier PN"
	1    4200 4400
	-1   0    0    1   
$EndComp
Wire Wire Line
	4200 2750 4200 2800
Wire Wire Line
	3850 2800 3850 2750
Wire Wire Line
	3850 2750 3500 2750
Wire Wire Line
	3500 2800 3500 2750
Connection ~ 3500 2750
Text Notes 900  1000 0    60   ~ 0
Color light sensor
Text Notes 2900 1000 0    60   ~ 0
Level shifting
Wire Notes Line
	850  850  850  3450
Wire Notes Line
	2750 3450 2750 850 
Wire Notes Line
	2750 850  850  850 
Text Notes 900  3700 0    60   ~ 0
Trigger button
Text Notes 2900 3700 0    60   ~ 0
Voltage regulator
Text Notes 5150 1000 0    60   ~ 0
Main board cable connection
Text Notes 5150 2450 0    50   ~ 0
Note: All signals on the cable are at 5V,\nand are level-shifted on this board\nas necessary.
Wire Notes Line
	850  3550 2250 3550
Wire Notes Line
	2250 3550 2250 4650
Wire Notes Line
	2250 4650 850  4650
Wire Notes Line
	850  4650 850  3550
Wire Notes Line
	2850 3550 2850 4800
Wire Notes Line
	2850 4800 5000 4800
Wire Notes Line
	5000 4800 5000 3550
Wire Notes Line
	5000 3550 2850 3550
Wire Notes Line
	5100 850  5100 2550
Wire Notes Line
	5100 2550 6950 2550
Wire Notes Line
	6950 2550 6950 850 
Wire Notes Line
	6950 850  5100 850 
Wire Notes Line
	850  3450 2750 3450
Wire Notes Line
	2850 850  2850 3450
Wire Notes Line
	2850 3450 5000 3450
Connection ~ 2450 1850
Connection ~ 2200 1950
Wire Notes Line
	2850 850  5000 850 
Wire Notes Line
	5000 850  5000 3450
$Comp
L power:PWR_FLAG #FLG0102
U 1 1 5F98ABC8
P 6250 1950
F 0 "#FLG0102" H 6250 2025 50  0001 C CNN
F 1 "PWR_FLAG" V 6250 2077 50  0001 L CNN
F 2 "" H 6250 1950 50  0001 C CNN
F 3 "~" H 6250 1950 50  0001 C CNN
	1    6250 1950
	0    1    1    0   
$EndComp
$Comp
L Device:C_Small C6
U 1 1 5F9A840E
P 5400 1600
F 0 "C6" H 5492 1646 50  0000 L CNN
F 1 "10uF" H 5492 1555 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 5400 1600 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 5400 1600 50  0001 C CNN
F 4 "CAP CER 10UF 16V X5R 0805" H 5400 1600 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 5400 1600 50  0001 C CNN "Manufacturer"
F 6 "CL21A106KOQNNNG" H 5400 1600 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5400 1600 50  0001 C CNN "Supplier"
F 8 "1276-6455-1-ND" H 5400 1600 50  0001 C CNN "Supplier PN"
	1    5400 1600
	1    0    0    -1  
$EndComp
Wire Wire Line
	6150 1600 6150 1650
Connection ~ 6150 1600
$Comp
L power:PWR_FLAG #FLG0101
U 1 1 5F9890D6
P 6150 1600
F 0 "#FLG0101" H 6150 1675 50  0001 C CNN
F 1 "PWR_FLAG" V 6150 1727 50  0001 L CNN
F 2 "" H 6150 1600 50  0001 C CNN
F 3 "~" H 6150 1600 50  0001 C CNN
	1    6150 1600
	0    -1   -1   0   
$EndComp
Wire Wire Line
	6150 1250 6150 1600
Connection ~ 6250 1950
$Comp
L power:GND #PWR0113
U 1 1 5F922E28
P 6250 1950
F 0 "#PWR0113" H 6250 1700 50  0001 C CNN
F 1 "GND" H 6255 1777 50  0000 C CNN
F 2 "" H 6250 1950 50  0001 C CNN
F 3 "" H 6250 1950 50  0001 C CNN
	1    6250 1950
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0112
U 1 1 5F922946
P 6150 1250
F 0 "#PWR0112" H 6150 1100 50  0001 C CNN
F 1 "+5V" H 6165 1423 50  0000 C CNN
F 2 "" H 6150 1250 50  0001 C CNN
F 3 "" H 6150 1250 50  0001 C CNN
	1    6150 1250
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5F9B4A6D
P 5400 1450
F 0 "#PWR?" H 5400 1300 50  0001 C CNN
F 1 "+5V" H 5415 1623 50  0000 C CNN
F 2 "" H 5400 1450 50  0001 C CNN
F 3 "" H 5400 1450 50  0001 C CNN
	1    5400 1450
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5F9B4E8D
P 5400 1750
F 0 "#PWR?" H 5400 1500 50  0001 C CNN
F 1 "GND" H 5405 1577 50  0000 C CNN
F 2 "" H 5400 1750 50  0001 C CNN
F 3 "" H 5400 1750 50  0001 C CNN
	1    5400 1750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5400 1700 5400 1750
Wire Wire Line
	5400 1450 5400 1500
Text Notes 7850 1450 0    50   ~ 0
Brown
Text Notes 7850 1550 0    50   ~ 0
Red
Text Notes 7850 1650 0    50   ~ 0
Orange
Text Notes 7850 1750 0    50   ~ 0
Yellow
Text Notes 7850 1850 0    50   ~ 0
Green
Text Notes 7850 1950 0    50   ~ 0
Black
Text Notes 7350 1450 0    50   ~ 0
1
Text Notes 7350 1550 0    50   ~ 0
2
Text Notes 7350 1650 0    50   ~ 0
3
Text Notes 7350 1750 0    50   ~ 0
4
Text Notes 7350 1850 0    50   ~ 0
5
Text Notes 7350 1950 0    50   ~ 0
6
Text Notes 7500 1450 0    50   ~ 0
SDA
Text Notes 7500 1550 0    50   ~ 0
~INT
Text Notes 7500 1650 0    50   ~ 0
GND
Text Notes 7500 1750 0    50   ~ 0
+5V
Text Notes 7500 1850 0    50   ~ 0
SCL
Text Notes 7500 1950 0    50   ~ 0
~BUTTON
Text Notes 7350 1300 0    50   ~ 0
Meter Cable Pinout
Wire Notes Line style solid
	7450 1325 7450 2000
Wire Notes Line style solid
	7300 2000 7300 1200
Wire Notes Line style solid
	7300 1200 8150 1200
Wire Notes Line style solid
	8150 1200 8150 2000
Wire Notes Line style solid
	7300 2000 8150 2000
Wire Notes Line style solid
	7300 1325 8150 1325
Wire Notes Line style solid
	7825 1325 7825 2000
$EndSCHEMATC
