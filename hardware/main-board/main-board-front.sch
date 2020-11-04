EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 4 4
Title "Printalyzer - Main Board (Front External Connections)"
Date ""
Rev ""
Comp "LogicProbe.org"
Comment1 "Derek Konigsberg"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L power:+5V #PWR?
U 1 1 5FA4F8DB
P 4250 950
AR Path="/5FA4F8DB" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8DB" Ref="#PWR0115"  Part="1" 
F 0 "#PWR0115" H 4250 800 50  0001 C CNN
F 1 "+5V" H 4265 1123 50  0000 C CNN
F 2 "" H 4250 950 50  0001 C CNN
F 3 "" H 4250 950 50  0001 C CNN
	1    4250 950 
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5FA4F8E1
P 6600 1900
AR Path="/5FA4F8E1" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8E1" Ref="#PWR0116"  Part="1" 
F 0 "#PWR0116" H 6600 1650 50  0001 C CNN
F 1 "GND" H 6605 1727 50  0000 C CNN
F 2 "" H 6600 1900 50  0001 C CNN
F 3 "" H 6600 1900 50  0001 C CNN
	1    6600 1900
	1    0    0    -1  
$EndComp
$Comp
L Interface_Expansion:P82B96 U?
U 1 1 5FA4F8E7
P 3150 1550
AR Path="/5FA4F8E7" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8E7" Ref="U9"  Part="1" 
F 0 "U9" H 3150 2167 50  0000 C CNN
F 1 "P82B96" H 3150 2076 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 3150 1550 50  0001 C CNN
F 3 "http://www.ti.com/general/docs/suppproductinfo.tsp?distId=10&gotoUrl=http%3A%2F%2Fwww.ti.com%2Flit%2Fgpn%2Fp82b96" H 3150 1550 50  0001 C CNN
F 4 "IC REDRIVER I2C 1CH 400KHZ 8SOIC" H 3150 1550 50  0001 C CNN "Description"
F 5 "Texas Instruments" H 3150 1550 50  0001 C CNN "Manufacturer"
F 6 "P82B96DR" H 3150 1550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 1550 50  0001 C CNN "Supplier"
F 8 "296-20942-1-ND" H 3150 1550 50  0001 C CNN "Supplier PN"
	1    3150 1550
	1    0    0    -1  
$EndComp
Wire Wire Line
	3750 1650 3750 1850
Connection ~ 3750 1850
Wire Wire Line
	3750 1250 3750 1450
$Comp
L Device:R_Small R?
U 1 1 5FA4F8F1
P 4250 1150
AR Path="/5FA4F8F1" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8F1" Ref="R10"  Part="1" 
F 0 "R10" H 4320 1196 50  0000 L CNN
F 1 "2K" H 4320 1105 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 4180 1150 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 4250 1150 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 4250 1150 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 4250 1150 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 4250 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4250 1150 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 4250 1150 50  0001 C CNN "Supplier PN"
	1    4250 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F8F7
P 4000 1150
AR Path="/5FA4F8F7" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8F7" Ref="R9"  Part="1" 
F 0 "R9" H 4070 1196 50  0000 L CNN
F 1 "2K" H 4070 1105 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3930 1150 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 4000 1150 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 4000 1150 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 4000 1150 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 4000 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4000 1150 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 4000 1150 50  0001 C CNN "Supplier PN"
	1    4000 1150
	1    0    0    -1  
$EndComp
Wire Wire Line
	4250 1250 4250 1850
Wire Wire Line
	4000 1050 4000 950 
Wire Wire Line
	4000 950  4250 950 
Wire Wire Line
	4250 1050 4250 950 
$Comp
L power:GND #PWR?
U 1 1 5FA4F901
P 2450 1950
AR Path="/5FA4F901" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F901" Ref="#PWR0117"  Part="1" 
F 0 "#PWR0117" H 2450 1700 50  0001 C CNN
F 1 "GND" H 2455 1777 50  0000 C CNN
F 2 "" H 2450 1950 50  0001 C CNN
F 3 "" H 2450 1950 50  0001 C CNN
	1    2450 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2550 1850 2450 1850
Wire Wire Line
	2450 1850 2450 1950
$Comp
L power:+5V #PWR?
U 1 1 5FA4F909
P 2450 1100
AR Path="/5FA4F909" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F909" Ref="#PWR0118"  Part="1" 
F 0 "#PWR0118" H 2450 950 50  0001 C CNN
F 1 "+5V" H 2465 1273 50  0000 C CNN
F 2 "" H 2450 1100 50  0001 C CNN
F 3 "" H 2450 1100 50  0001 C CNN
	1    2450 1100
	1    0    0    -1  
$EndComp
Wire Wire Line
	2450 1100 2450 1250
Wire Wire Line
	2450 1250 2550 1250
$Comp
L Device:C_Small C?
U 1 1 5FA4F911
P 2350 1100
AR Path="/5FA4F911" Ref="C?"  Part="1" 
AR Path="/5FA19AFF/5FA4F911" Ref="C26"  Part="1" 
F 0 "C26" H 2465 1146 50  0000 L CNN
F 1 "0.1uF" H 2465 1055 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2388 950 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 2350 1100 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 2350 1100 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 2350 1100 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 2350 1100 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2350 1100 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 2350 1100 50  0001 C CNN "Supplier PN"
	1    2350 1100
	0    1    1    0   
$EndComp
Connection ~ 2450 1100
$Comp
L power:GND #PWR?
U 1 1 5FA4F918
P 2150 1150
AR Path="/5FA4F918" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F918" Ref="#PWR0119"  Part="1" 
F 0 "#PWR0119" H 2150 900 50  0001 C CNN
F 1 "GND" H 2155 977 50  0000 C CNN
F 2 "" H 2150 1150 50  0001 C CNN
F 3 "" H 2150 1150 50  0001 C CNN
	1    2150 1150
	1    0    0    -1  
$EndComp
Wire Wire Line
	2250 1100 2150 1100
Wire Wire Line
	2150 1100 2150 1150
Wire Wire Line
	2550 1450 1600 1450
Wire Wire Line
	2550 1650 1850 1650
$Comp
L Device:R_Small R?
U 1 1 5FA4F924
P 1850 1250
AR Path="/5FA4F924" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F924" Ref="R12"  Part="1" 
F 0 "R12" H 1920 1296 50  0000 L CNN
F 1 "2K" H 1920 1205 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 1780 1250 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1850 1250 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 1850 1250 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1850 1250 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 1850 1250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1850 1250 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 1850 1250 50  0001 C CNN "Supplier PN"
	1    1850 1250
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F92A
P 1600 1250
AR Path="/5FA4F92A" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F92A" Ref="R11"  Part="1" 
F 0 "R11" H 1670 1296 50  0000 L CNN
F 1 "2K" H 1670 1205 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 1530 1250 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1600 1250 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 1600 1250 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1600 1250 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 1600 1250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1600 1250 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 1600 1250 50  0001 C CNN "Supplier PN"
	1    1600 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	1850 1350 1850 1650
Connection ~ 1850 1650
Wire Wire Line
	1850 1650 1350 1650
Wire Wire Line
	1600 1350 1600 1450
Connection ~ 1600 1450
Wire Wire Line
	1600 1450 1350 1450
$Comp
L power:+3.3V #PWR?
U 1 1 5FA4F936
P 1850 1050
AR Path="/5FA4F936" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F936" Ref="#PWR0120"  Part="1" 
F 0 "#PWR0120" H 1850 900 50  0001 C CNN
F 1 "+3.3V" H 1865 1223 50  0000 C CNN
F 2 "" H 1850 1050 50  0001 C CNN
F 3 "" H 1850 1050 50  0001 C CNN
	1    1850 1050
	1    0    0    -1  
$EndComp
Wire Wire Line
	1600 1150 1600 1050
Wire Wire Line
	1600 1050 1850 1050
Wire Wire Line
	1850 1150 1850 1050
Connection ~ 1850 1050
$Comp
L Project:Conn_CUI_MD-60SM J?
U 1 1 5FA4F940
P 6650 1550
AR Path="/5FA4F940" Ref="J?"  Part="1" 
AR Path="/5FA19AFF/5FA4F940" Ref="J5"  Part="1" 
F 0 "J5" H 6650 1965 50  0000 C CNN
F 1 "Meter Probe" H 6650 1874 50  0000 C CNN
F 2 "lib_fp:CUI_MD-60SM" H 6650 950 50  0001 C CNN
F 3 "https://www.cuidevices.com/product/resource/digikeypdf/md-sm-series.pdf" H 6650 1550 50  0001 C CNN
F 4 "CONN RCPT FMALE MINI DIN 6P SLDR" H 6650 1550 50  0001 C CNN "Description"
F 5 "CUI Devices" H 6650 1550 50  0001 C CNN "Manufacturer"
F 6 "MD-60SM" H 6650 1550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 6650 1550 50  0001 C CNN "Supplier"
F 8 "CP-2260-ND" H 6650 1550 50  0001 C CNN "Supplier PN"
	1    6650 1550
	1    0    0    -1  
$EndComp
Wire Wire Line
	6700 1900 6600 1900
Connection ~ 6600 1900
Connection ~ 4250 950 
$Comp
L power:+5V #PWR?
U 1 1 5FA4F949
P 4750 950
AR Path="/5FA4F949" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F949" Ref="#PWR0121"  Part="1" 
F 0 "#PWR0121" H 4750 800 50  0001 C CNN
F 1 "+5V" H 4765 1123 50  0000 C CNN
F 2 "" H 4750 950 50  0001 C CNN
F 3 "" H 4750 950 50  0001 C CNN
	1    4750 950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	6150 1150 6150 1550
Wire Wire Line
	6150 1550 6250 1550
Wire Wire Line
	7050 1550 7150 1550
Wire Wire Line
	7150 1550 7150 1900
$Comp
L power:GND #PWR?
U 1 1 5FA4F953
P 7150 1900
AR Path="/5FA4F953" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F953" Ref="#PWR0122"  Part="1" 
F 0 "#PWR0122" H 7150 1650 50  0001 C CNN
F 1 "GND" H 7155 1727 50  0000 C CNN
F 2 "" H 7150 1900 50  0001 C CNN
F 3 "" H 7150 1900 50  0001 C CNN
	1    7150 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3750 1850 4250 1850
Wire Wire Line
	3750 1450 4000 1450
Wire Wire Line
	5900 900  7250 900 
Wire Wire Line
	7250 900  7250 1650
Wire Wire Line
	7250 1650 7050 1650
Connection ~ 3750 1450
Wire Wire Line
	4000 1250 4000 1450
Wire Wire Line
	5950 2150 5950 1000
Wire Wire Line
	5950 1000 7200 1000
Wire Wire Line
	7200 1000 7200 1450
Wire Wire Line
	7200 1450 7050 1450
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5FA4F969
P 3150 2550
AR Path="/5FA4F969" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5FA4F969" Ref="Q1"  Part="1" 
F 0 "Q1" V 3399 2550 50  0000 C CNN
F 1 "BSS138" V 3490 2550 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3350 2475 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3150 2550 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3150 2550 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3150 2550 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3150 2550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 2550 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3150 2550 50  0001 C CNN "Supplier PN"
	1    3150 2550
	0    1    1    0   
$EndComp
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5FA4F96F
P 3150 3300
AR Path="/5FA4F96F" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5FA4F96F" Ref="Q2"  Part="1" 
F 0 "Q2" V 3399 3300 50  0000 C CNN
F 1 "BSS138" V 3490 3300 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3350 3225 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3150 3300 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3150 3300 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3150 3300 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3150 3300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 3300 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3150 3300 50  0001 C CNN "Supplier PN"
	1    3150 3300
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F975
P 2800 2450
AR Path="/5FA4F975" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F975" Ref="R13"  Part="1" 
F 0 "R13" H 2870 2496 50  0000 L CNN
F 1 "10K" H 2870 2405 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2730 2450 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2800 2450 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2800 2450 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2800 2450 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2800 2450 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2800 2450 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2800 2450 50  0001 C CNN "Supplier PN"
	1    2800 2450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F97B
P 3500 2450
AR Path="/5FA4F97B" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F97B" Ref="R14"  Part="1" 
F 0 "R14" H 3570 2496 50  0000 L CNN
F 1 "10K" H 3570 2405 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3430 2450 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3500 2450 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3500 2450 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3500 2450 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3500 2450 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3500 2450 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3500 2450 50  0001 C CNN "Supplier PN"
	1    3500 2450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F981
P 2800 3200
AR Path="/5FA4F981" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F981" Ref="R15"  Part="1" 
F 0 "R15" H 2870 3246 50  0000 L CNN
F 1 "10K" H 2870 3155 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2730 3200 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2800 3200 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2800 3200 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2800 3200 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2800 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2800 3200 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2800 3200 50  0001 C CNN "Supplier PN"
	1    2800 3200
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F987
P 3500 3200
AR Path="/5FA4F987" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F987" Ref="R16"  Part="1" 
F 0 "R16" H 3570 3246 50  0000 L CNN
F 1 "10K" H 3570 3155 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3430 3200 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3500 3200 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3500 3200 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3500 3200 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3500 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3500 3200 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3500 3200 50  0001 C CNN "Supplier PN"
	1    3500 3200
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA4F98D
P 3500 2300
AR Path="/5FA4F98D" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F98D" Ref="#PWR0123"  Part="1" 
F 0 "#PWR0123" H 3500 2150 50  0001 C CNN
F 1 "+5V" H 3515 2473 50  0000 C CNN
F 2 "" H 3500 2300 50  0001 C CNN
F 3 "" H 3500 2300 50  0001 C CNN
	1    3500 2300
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA4F993
P 2800 2300
AR Path="/5FA4F993" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F993" Ref="#PWR0124"  Part="1" 
F 0 "#PWR0124" H 2800 2150 50  0001 C CNN
F 1 "+3.3V" H 2815 2473 50  0000 C CNN
F 2 "" H 2800 2300 50  0001 C CNN
F 3 "" H 2800 2300 50  0001 C CNN
	1    2800 2300
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA4F999
P 2800 3050
AR Path="/5FA4F999" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F999" Ref="#PWR0125"  Part="1" 
F 0 "#PWR0125" H 2800 2900 50  0001 C CNN
F 1 "+3.3V" H 2815 3223 50  0000 C CNN
F 2 "" H 2800 3050 50  0001 C CNN
F 3 "" H 2800 3050 50  0001 C CNN
	1    2800 3050
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA4F99F
P 3500 3050
AR Path="/5FA4F99F" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F99F" Ref="#PWR0126"  Part="1" 
F 0 "#PWR0126" H 3500 2900 50  0001 C CNN
F 1 "+5V" H 3515 3223 50  0000 C CNN
F 2 "" H 3500 3050 50  0001 C CNN
F 3 "" H 3500 3050 50  0001 C CNN
	1    3500 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	2950 2650 2800 2650
Wire Wire Line
	2800 2650 2800 2550
Wire Wire Line
	3350 2650 3500 2650
Wire Wire Line
	3500 2650 3500 2550
Wire Wire Line
	2800 3300 2800 3400
Wire Wire Line
	2800 3400 2950 3400
Wire Wire Line
	3500 3300 3500 3400
Wire Wire Line
	3500 3400 3350 3400
Wire Wire Line
	3500 3050 3500 3100
Wire Wire Line
	2800 3050 2800 3100
Wire Wire Line
	3500 2300 3500 2350
Wire Wire Line
	2800 2300 2800 2350
Wire Wire Line
	3150 2350 3150 2300
Wire Wire Line
	3150 2300 2800 2300
Connection ~ 2800 2300
Wire Wire Line
	3150 3100 3150 3050
Wire Wire Line
	3150 3050 2800 3050
Connection ~ 2800 3050
Wire Wire Line
	6250 1450 6000 1450
Wire Wire Line
	6000 1450 6000 2650
Wire Wire Line
	6050 2750 6050 1650
Wire Wire Line
	6050 1650 6250 1650
Wire Wire Line
	2600 2650 2800 2650
Connection ~ 2800 2650
Wire Wire Line
	2600 3400 2800 3400
Connection ~ 2800 3400
$Comp
L Power_Protection:USBLC6-2SC6 U?
U 1 1 5FA5B3D0
P 3750 6550
AR Path="/5FA5B3D0" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3D0" Ref="U12"  Part="1" 
F 0 "U12" V 4100 6300 50  0000 R CNN
F 1 "USBLC6-2SC6" V 3400 6400 50  0000 R CNN
F 2 "Package_TO_SOT_SMD:SOT-23-6" H 3750 6050 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/usblc6-2.pdf" H 3950 6900 50  0001 C CNN
F 4 "TVS DIODE 5.25V 17V SOT23-6" H 3750 6550 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3750 6550 50  0001 C CNN "Manufacturer"
F 6 "USBLC6-2SC6" H 3750 6550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3750 6550 50  0001 C CNN "Supplier"
F 8 "497-5235-1-ND" H 3750 6550 50  0001 C CNN "Supplier PN"
	1    3750 6550
	0    -1   -1   0   
$EndComp
$Comp
L Project:STMPS2141 U?
U 1 1 5FA5B3D6
P 2450 5950
AR Path="/5FA5B3D6" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3D6" Ref="U11"  Part="1" 
F 0 "U11" H 2450 6317 50  0000 C CNN
F 1 "STMPS2141" H 2450 6226 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23-5" H 2450 6300 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/stmps2141.pdf" H 2250 6200 50  0001 C CNN
F 4 "IC PWR SWITCH N-CHAN 1:1 SOT23-5" H 2450 5950 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 2450 5950 50  0001 C CNN "Manufacturer"
F 6 "STMPS2141STR" H 2450 5950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2450 5950 50  0001 C CNN "Supplier"
F 8 "497-6933-1-ND" H 2450 5950 50  0001 C CNN "Supplier PN"
	1    2450 5950
	1    0    0    -1  
$EndComp
$Comp
L Connector:USB_A J?
U 1 1 5FA5B3DC
P 5150 6550
AR Path="/5FA5B3DC" Ref="J?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3DC" Ref="J7"  Part="1" 
F 0 "J7" H 4920 6539 50  0000 R CNN
F 1 "USB_A" H 4920 6448 50  0000 R CNN
F 2 "Connector_USB:USB_A_Molex_67643_Horizontal" H 5300 6500 50  0001 C CNN
F 3 "https://www.molex.com/pdm_docs/sd/676430910_sd.pdf" H 5300 6500 50  0001 C CNN
F 4 "CONN RCPT TYPEA 4POS R/A" H 5150 6550 50  0001 C CNN "Description"
F 5 "Molex" H 5150 6550 50  0001 C CNN "Manufacturer"
F 6 "0676432910" H 5150 6550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5150 6550 50  0001 C CNN "Supplier"
F 8 "WM11258-ND" H 5150 6550 50  0001 C CNN "Supplier PN"
	1    5150 6550
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3850 6950 3850 7050
Wire Wire Line
	3850 7050 4600 7050
Wire Wire Line
	4600 7050 4600 6650
Wire Wire Line
	4600 6650 4850 6650
Wire Wire Line
	4600 6050 4600 6550
Wire Wire Line
	4600 6550 4850 6550
Wire Wire Line
	4600 6050 3850 6050
Wire Wire Line
	3850 6050 3850 6150
$Comp
L power:GND #PWR?
U 1 1 5FA5B3EA
P 4150 6600
AR Path="/5FA5B3EA" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3EA" Ref="#PWR0127"  Part="1" 
F 0 "#PWR0127" H 4150 6350 50  0001 C CNN
F 1 "GND" H 4155 6427 50  0000 C CNN
F 2 "" H 4150 6600 50  0001 C CNN
F 3 "" H 4150 6600 50  0001 C CNN
	1    4150 6600
	1    0    0    -1  
$EndComp
Wire Wire Line
	4150 6550 4150 6600
Wire Wire Line
	3350 6550 3150 6550
Wire Wire Line
	3150 6550 3150 6600
$Comp
L power:GND #PWR?
U 1 1 5FA5B3FA
P 3150 6800
AR Path="/5FA5B3FA" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3FA" Ref="#PWR0128"  Part="1" 
F 0 "#PWR0128" H 3150 6550 50  0001 C CNN
F 1 "GND" H 3155 6627 50  0000 C CNN
F 2 "" H 3150 6800 50  0001 C CNN
F 3 "" H 3150 6800 50  0001 C CNN
	1    3150 6800
	1    0    0    -1  
$EndComp
Wire Wire Line
	3350 6550 3350 5950
Wire Wire Line
	3350 5950 4850 5950
Wire Wire Line
	4850 5950 4850 6350
Connection ~ 3350 6550
$Comp
L Device:R_Small R?
U 1 1 5FA5B407
P 2950 5550
AR Path="/5FA5B407" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA5B407" Ref="R19"  Part="1" 
F 0 "R19" H 3020 5596 50  0000 L CNN
F 1 "47K" H 3020 5505 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2880 5550 50  0001 C CNN
F 3 "https://www.vishay.com/docs/28773/crcwce3.pdf" H 2950 5550 50  0001 C CNN
F 4 "RES 47K OHM 5% 1/10W 0603" H 2950 5550 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2950 5550 50  0001 C CNN "Manufacturer"
F 6 "CRCW060347K0JNEAC" H 2950 5550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2950 5550 50  0001 C CNN "Supplier"
F 8 "541-4061-1-ND" H 2950 5550 50  0001 C CNN "Supplier PN"
	1    2950 5550
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA5B40D
P 2950 5400
AR Path="/5FA5B40D" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B40D" Ref="#PWR0129"  Part="1" 
F 0 "#PWR0129" H 2950 5250 50  0001 C CNN
F 1 "+3.3V" H 2965 5573 50  0000 C CNN
F 2 "" H 2950 5400 50  0001 C CNN
F 3 "" H 2950 5400 50  0001 C CNN
	1    2950 5400
	1    0    0    -1  
$EndComp
Wire Wire Line
	2950 5400 2950 5450
Wire Wire Line
	2950 5650 2950 6050
Wire Wire Line
	2950 6050 2850 6050
$Comp
L Device:R_Small R?
U 1 1 5FA5B416
P 1800 5850
AR Path="/5FA5B416" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA5B416" Ref="R20"  Part="1" 
F 0 "R20" H 1870 5896 50  0000 L CNN
F 1 "10K" H 1870 5805 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 1730 5850 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1800 5850 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 1800 5850 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1800 5850 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 1800 5850 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1800 5850 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 1800 5850 50  0001 C CNN "Supplier PN"
	1    1800 5850
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA5B41C
P 2050 5650
AR Path="/5FA5B41C" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B41C" Ref="#PWR0130"  Part="1" 
F 0 "#PWR0130" H 2050 5500 50  0001 C CNN
F 1 "+5V" H 2065 5823 50  0000 C CNN
F 2 "" H 2050 5650 50  0001 C CNN
F 3 "" H 2050 5650 50  0001 C CNN
	1    2050 5650
	1    0    0    -1  
$EndComp
Wire Wire Line
	2050 5650 2050 5850
Wire Wire Line
	1800 5950 1800 6050
Wire Wire Line
	1800 6050 2050 6050
$Comp
L power:+3.3V #PWR?
U 1 1 5FA5B425
P 1800 5700
AR Path="/5FA5B425" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B425" Ref="#PWR0131"  Part="1" 
F 0 "#PWR0131" H 1800 5550 50  0001 C CNN
F 1 "+3.3V" H 1815 5873 50  0000 C CNN
F 2 "" H 1800 5700 50  0001 C CNN
F 3 "" H 1800 5700 50  0001 C CNN
	1    1800 5700
	1    0    0    -1  
$EndComp
Wire Wire Line
	1800 5700 1800 5750
Wire Wire Line
	1650 6050 1800 6050
Connection ~ 1800 6050
Wire Wire Line
	1650 6300 2950 6300
Wire Wire Line
	2950 6300 2950 6050
Connection ~ 2950 6050
$Comp
L power:GND #PWR?
U 1 1 5FA5B433
P 2450 6350
AR Path="/5FA5B433" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B433" Ref="#PWR0132"  Part="1" 
F 0 "#PWR0132" H 2450 6100 50  0001 C CNN
F 1 "GND" H 2455 6177 50  0000 C CNN
F 2 "" H 2450 6350 50  0001 C CNN
F 3 "" H 2450 6350 50  0001 C CNN
	1    2450 6350
	1    0    0    -1  
$EndComp
Wire Wire Line
	2450 6250 2450 6350
$Comp
L power:GND #PWR?
U 1 1 5FA5B43A
P 5150 7050
AR Path="/5FA5B43A" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B43A" Ref="#PWR0133"  Part="1" 
F 0 "#PWR0133" H 5150 6800 50  0001 C CNN
F 1 "GND" H 5155 6877 50  0000 C CNN
F 2 "" H 5150 7050 50  0001 C CNN
F 3 "" H 5150 7050 50  0001 C CNN
	1    5150 7050
	1    0    0    -1  
$EndComp
Wire Wire Line
	5250 6950 5150 6950
Wire Wire Line
	5150 6950 5150 7050
Connection ~ 5150 6950
Text Notes 5000 4600 0    50   ~ 0
Follow datasheet layout guidance for\nall ESD suppression components.
Wire Wire Line
	2850 5850 3150 5850
Wire Wire Line
	3150 5850 3150 6550
Connection ~ 3150 6550
Wire Wire Line
	3650 6150 3000 6150
Wire Wire Line
	3000 6150 3000 6650
Wire Wire Line
	3000 6650 1650 6650
Wire Wire Line
	3650 6950 3650 7050
Wire Wire Line
	3000 6850 1650 6850
$Comp
L Device:C_Small C27
U 1 1 5F988495
P 3150 6700
F 0 "C27" H 3242 6746 50  0000 L CNN
F 1 "0.1uF" H 3242 6655 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 3150 6700 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 3150 6700 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 3150 6700 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 3150 6700 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 3150 6700 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 6700 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 3150 6700 50  0001 C CNN "Supplier PN"
	1    3150 6700
	1    0    0    -1  
$EndComp
Wire Wire Line
	3650 7050 3000 7050
Wire Wire Line
	3000 7050 3000 6850
$Comp
L Connector_Generic:Conn_01x02 J6
U 1 1 5F9A4F1A
P 6450 2900
F 0 "J6" H 6400 3000 50  0000 L CNN
F 1 "Footswitch" H 6400 2700 50  0000 L CNN
F 2 "Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical" H 6450 2900 50  0001 C CNN
F 3 "http://www.jst-mfg.com/product/pdf/eng/ePH.pdf" H 6450 2900 50  0001 C CNN
F 4 "CONN HEADER VERT 2POS 2MM" H 6450 2900 50  0001 C CNN "Description"
F 5 "JST Sales America Inc." H 6450 2900 50  0001 C CNN "Manufacturer"
F 6 "B2B-PH-K-S(LF)(SN)" H 6450 2900 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 6450 2900 50  0001 C CNN "Supplier"
F 8 "455-1704-ND" H 6450 2900 50  0001 C CNN "Supplier PN"
	1    6450 2900
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5F9B6C01
P 6150 3100
AR Path="/5F9B6C01" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5F9B6C01" Ref="#PWR071"  Part="1" 
F 0 "#PWR071" H 6150 2850 50  0001 C CNN
F 1 "GND" H 6155 2927 50  0000 C CNN
F 2 "" H 6150 3100 50  0001 C CNN
F 3 "" H 6150 3100 50  0001 C CNN
	1    6150 3100
	1    0    0    -1  
$EndComp
Wire Wire Line
	6250 3000 6150 3000
Wire Wire Line
	6150 3000 6150 3100
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5F9C71D7
P 3150 4050
AR Path="/5F9C71D7" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5F9C71D7" Ref="Q3"  Part="1" 
F 0 "Q3" V 3399 4050 50  0000 C CNN
F 1 "BSS138" V 3490 4050 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3350 3975 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3150 4050 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3150 4050 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3150 4050 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3150 4050 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3150 4050 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3150 4050 50  0001 C CNN "Supplier PN"
	1    3150 4050
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5F9C76EB
P 2800 3950
AR Path="/5F9C76EB" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5F9C76EB" Ref="R17"  Part="1" 
F 0 "R17" H 2870 3996 50  0000 L CNN
F 1 "10K" H 2870 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2730 3950 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2800 3950 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2800 3950 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2800 3950 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2800 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2800 3950 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2800 3950 50  0001 C CNN "Supplier PN"
	1    2800 3950
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5F9C76F5
P 3500 3950
AR Path="/5F9C76F5" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5F9C76F5" Ref="R18"  Part="1" 
F 0 "R18" H 3570 3996 50  0000 L CNN
F 1 "10K" H 3570 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3430 3950 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3500 3950 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3500 3950 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3500 3950 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3500 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3500 3950 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3500 3950 50  0001 C CNN "Supplier PN"
	1    3500 3950
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5F9C76FF
P 2800 3800
AR Path="/5F9C76FF" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5F9C76FF" Ref="#PWR073"  Part="1" 
F 0 "#PWR073" H 2800 3650 50  0001 C CNN
F 1 "+3.3V" H 2815 3973 50  0000 C CNN
F 2 "" H 2800 3800 50  0001 C CNN
F 3 "" H 2800 3800 50  0001 C CNN
	1    2800 3800
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5F9C7709
P 3500 3800
AR Path="/5F9C7709" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5F9C7709" Ref="#PWR074"  Part="1" 
F 0 "#PWR074" H 3500 3650 50  0001 C CNN
F 1 "+5V" H 3515 3973 50  0000 C CNN
F 2 "" H 3500 3800 50  0001 C CNN
F 3 "" H 3500 3800 50  0001 C CNN
	1    3500 3800
	1    0    0    -1  
$EndComp
Wire Wire Line
	2800 4050 2800 4150
Wire Wire Line
	2800 4150 2950 4150
Wire Wire Line
	3500 4050 3500 4150
Wire Wire Line
	3500 4150 3350 4150
Wire Wire Line
	3500 3800 3500 3850
Wire Wire Line
	2800 3800 2800 3850
Wire Wire Line
	3150 3850 3150 3800
Wire Wire Line
	3150 3800 2800 3800
Connection ~ 2800 3800
Wire Wire Line
	2600 4150 2800 4150
Connection ~ 2800 4150
$Comp
L Power_Protection:USBLC6-2SC6 U?
U 1 1 5F9E0AB3
P 5150 1700
AR Path="/5F9E0AB3" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5F9E0AB3" Ref="U10"  Part="1" 
F 0 "U10" V 5500 1450 50  0000 R CNN
F 1 "USBLC6-2SC6" V 4800 1550 50  0000 R CNN
F 2 "Package_TO_SOT_SMD:SOT-23-6" H 5150 1200 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/usblc6-2.pdf" H 5350 2050 50  0001 C CNN
F 4 "TVS DIODE 5.25V 17V SOT23-6" H 5150 1700 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 5150 1700 50  0001 C CNN "Manufacturer"
F 6 "USBLC6-2SC6" H 5150 1700 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5150 1700 50  0001 C CNN "Supplier"
F 8 "497-5235-1-ND" H 5150 1700 50  0001 C CNN "Supplier PN"
	1    5150 1700
	0    -1   -1   0   
$EndComp
Wire Wire Line
	5900 1250 5900 900 
Wire Wire Line
	5900 1250 5250 1250
Wire Wire Line
	5250 1250 5250 1300
Wire Wire Line
	5050 1300 5050 1250
Wire Wire Line
	5050 1250 4550 1250
Wire Wire Line
	4550 1250 4550 1450
Wire Wire Line
	4550 1450 4000 1450
Connection ~ 4000 1450
Wire Wire Line
	5250 2100 5250 2150
Wire Wire Line
	5250 2150 5950 2150
Wire Wire Line
	5050 2100 5050 2150
Wire Wire Line
	5050 2150 4500 2150
Wire Wire Line
	4500 2150 4500 1850
Wire Wire Line
	4500 1850 4250 1850
Connection ~ 4250 1850
$Comp
L power:GND #PWR?
U 1 1 5FA27F45
P 5550 1750
AR Path="/5FA27F45" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA27F45" Ref="#PWR070"  Part="1" 
F 0 "#PWR070" H 5550 1500 50  0001 C CNN
F 1 "GND" H 5555 1577 50  0000 C CNN
F 2 "" H 5550 1750 50  0001 C CNN
F 3 "" H 5550 1750 50  0001 C CNN
	1    5550 1750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5550 1700 5550 1750
Wire Wire Line
	6150 1150 4750 1150
Wire Wire Line
	4750 1150 4750 1700
Wire Wire Line
	4750 950  4750 1150
Connection ~ 4750 1150
Wire Wire Line
	3500 2650 5000 2650
Connection ~ 3500 2650
Wire Wire Line
	6050 2750 5100 2750
Wire Wire Line
	3800 2750 3800 3400
Wire Wire Line
	3800 3400 3500 3400
Connection ~ 3500 3400
Wire Wire Line
	6250 2900 5200 2900
Wire Wire Line
	3950 2900 3950 4150
Wire Wire Line
	3950 4150 3500 4150
Connection ~ 3500 4150
Wire Wire Line
	5000 3100 5000 2650
Connection ~ 5000 2650
Wire Wire Line
	5000 2650 6000 2650
Wire Wire Line
	5100 3100 5100 2750
Connection ~ 5100 2750
Wire Wire Line
	5100 2750 3800 2750
Wire Wire Line
	5200 3100 5200 2900
Connection ~ 5200 2900
Wire Wire Line
	5200 2900 3950 2900
NoConn ~ 5300 3100
$Comp
L power:GND #PWR?
U 1 1 5FA61E9F
P 5200 3750
AR Path="/5FA61E9F" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA61E9F" Ref="#PWR072"  Part="1" 
F 0 "#PWR072" H 5200 3500 50  0001 C CNN
F 1 "GND" H 5205 3577 50  0000 C CNN
F 2 "" H 5200 3750 50  0001 C CNN
F 3 "" H 5200 3750 50  0001 C CNN
	1    5200 3750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5200 3700 5200 3750
Text HLabel 1350 1450 0    50   BiDi ~ 0
EXT_I2C_SDA
Text HLabel 1350 1650 0    50   BiDi ~ 0
EXT_I2C_SCL
Text HLabel 2600 2650 0    50   Output ~ 0
SENSOR_BTN
Text HLabel 2600 3400 0    50   Output ~ 0
SENSOR_INT
Text HLabel 2600 4150 0    50   Output ~ 0
FOOTSWITCH
Text HLabel 1650 6050 0    50   Input ~ 0
USB_DRIVE_VBUS
Text HLabel 1650 6300 0    50   Output ~ 0
USB_VBUS_OC
Text HLabel 1650 6650 0    50   BiDi ~ 0
USB_DP
Text HLabel 1650 6850 0    50   BiDi ~ 0
USB_DM
Text Notes 7550 1150 0    50   ~ 0
Meter Pinout
Text Notes 7650 1300 2    50   ~ 0
1
Text Notes 7650 1400 2    50   ~ 0
2
Text Notes 7650 1500 2    50   ~ 0
3
Text Notes 7650 1600 2    50   ~ 0
4
Text Notes 7650 1700 2    50   ~ 0
5
Text Notes 7650 1800 2    50   ~ 0
6
Text Notes 7750 1300 0    50   ~ 0
SDA
Text Notes 7750 1400 0    50   ~ 0
~INT
Text Notes 7750 1500 0    50   ~ 0
GND
Text Notes 7750 1600 0    50   ~ 0
+5V
Text Notes 7750 1700 0    50   ~ 0
SCL
Text Notes 7750 1800 0    50   ~ 0
~BUTTON
Wire Notes Line style solid
	7500 1050 7500 1850
Wire Notes Line style solid
	7500 1850 8100 1850
Wire Notes Line style solid
	8100 1850 8100 1050
Wire Notes Line style solid
	8100 1050 7500 1050
Wire Notes Line style solid
	7500 1175 8100 1175
Wire Notes Line style solid
	7700 1175 7700 1850
$Comp
L Project:DT2636-04S D13
U 1 1 5FAA8A21
P 5200 3400
F 0 "D13" H 5480 3446 50  0000 L CNN
F 1 "DT2636-04S" H 5480 3355 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-363_SC-70-6" H 5500 3350 50  0001 L CNN
F 3 "https://www.diodes.com/assets/Datasheets/DT2636-04S.pdf" H 5525 3075 50  0001 C CNN
F 4 "TVS DIODE 5.5V 9V SOT363" H 5200 3400 50  0001 C CNN "Description"
F 5 "Diodes Incorporated" H 5200 3400 50  0001 C CNN "Manufacturer"
F 6 "DT2636-04S-7" H 5200 3400 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5200 3400 50  0001 C CNN "Supplier"
F 8 "DT2636-04S-7DICT-ND" H 5200 3400 50  0001 C CNN "Supplier PN"
	1    5200 3400
	1    0    0    -1  
$EndComp
$EndSCHEMATC
