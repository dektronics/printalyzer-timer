EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 4 4
Title "Printalyzer - Main Board (Front External Connections)"
Date "2020-11-16"
Rev "A"
Comp "Dektronics, Inc."
Comment1 "Derek Konigsberg"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L power:+5V #PWR?
U 1 1 5FA4F8DB
P 4150 950
AR Path="/5FA4F8DB" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8DB" Ref="#PWR0115"  Part="1" 
F 0 "#PWR0115" H 4150 800 50  0001 C CNN
F 1 "+5V" H 4165 1123 50  0000 C CNN
F 2 "" H 4150 950 50  0001 C CNN
F 3 "" H 4150 950 50  0001 C CNN
	1    4150 950 
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5FA4F8E1
P 6500 1900
AR Path="/5FA4F8E1" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8E1" Ref="#PWR0116"  Part="1" 
F 0 "#PWR0116" H 6500 1650 50  0001 C CNN
F 1 "GND" H 6505 1727 50  0000 C CNN
F 2 "" H 6500 1900 50  0001 C CNN
F 3 "" H 6500 1900 50  0001 C CNN
	1    6500 1900
	1    0    0    -1  
$EndComp
$Comp
L Interface_Expansion:P82B96 U?
U 1 1 5FA4F8E7
P 3050 1550
AR Path="/5FA4F8E7" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8E7" Ref="U9"  Part="1" 
F 0 "U9" H 3050 2167 50  0000 C CNN
F 1 "P82B96" H 3050 2076 50  0000 C CNN
F 2 "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm" H 3050 1550 50  0001 C CNN
F 3 "http://www.ti.com/general/docs/suppproductinfo.tsp?distId=10&gotoUrl=http%3A%2F%2Fwww.ti.com%2Flit%2Fgpn%2Fp82b96" H 3050 1550 50  0001 C CNN
F 4 "IC REDRIVER I2C 1CH 400KHZ 8SOIC" H 3050 1550 50  0001 C CNN "Description"
F 5 "Texas Instruments" H 3050 1550 50  0001 C CNN "Manufacturer"
F 6 "P82B96DR" H 3050 1550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3050 1550 50  0001 C CNN "Supplier"
F 8 "296-20942-1-ND" H 3050 1550 50  0001 C CNN "Supplier PN"
	1    3050 1550
	1    0    0    -1  
$EndComp
Wire Wire Line
	3650 1650 3650 1850
Connection ~ 3650 1850
Wire Wire Line
	3650 1250 3650 1450
$Comp
L Device:R_Small R?
U 1 1 5FA4F8F1
P 4150 1150
AR Path="/5FA4F8F1" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8F1" Ref="R10"  Part="1" 
F 0 "R10" H 4200 1200 50  0000 L CNN
F 1 "2K" H 4200 1100 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 4080 1150 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 4150 1150 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 4150 1150 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 4150 1150 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 4150 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4150 1150 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 4150 1150 50  0001 C CNN "Supplier PN"
	1    4150 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F8F7
P 3900 1150
AR Path="/5FA4F8F7" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8F7" Ref="R9"  Part="1" 
F 0 "R9" H 3950 1200 50  0000 L CNN
F 1 "2K" H 3950 1100 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3830 1150 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3900 1150 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 3900 1150 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3900 1150 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 3900 1150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3900 1150 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 3900 1150 50  0001 C CNN "Supplier PN"
	1    3900 1150
	1    0    0    -1  
$EndComp
Wire Wire Line
	4150 1250 4150 1850
Wire Wire Line
	3900 1050 3900 950 
Wire Wire Line
	3900 950  4150 950 
Wire Wire Line
	4150 1050 4150 950 
$Comp
L power:GND #PWR?
U 1 1 5FA4F901
P 2350 1950
AR Path="/5FA4F901" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F901" Ref="#PWR0117"  Part="1" 
F 0 "#PWR0117" H 2350 1700 50  0001 C CNN
F 1 "GND" H 2355 1777 50  0000 C CNN
F 2 "" H 2350 1950 50  0001 C CNN
F 3 "" H 2350 1950 50  0001 C CNN
	1    2350 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2450 1850 2350 1850
Wire Wire Line
	2350 1850 2350 1950
$Comp
L power:+5V #PWR?
U 1 1 5FA4F909
P 2350 1100
AR Path="/5FA4F909" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F909" Ref="#PWR0118"  Part="1" 
F 0 "#PWR0118" H 2350 950 50  0001 C CNN
F 1 "+5V" H 2365 1273 50  0000 C CNN
F 2 "" H 2350 1100 50  0001 C CNN
F 3 "" H 2350 1100 50  0001 C CNN
	1    2350 1100
	1    0    0    -1  
$EndComp
Wire Wire Line
	2350 1100 2350 1250
Wire Wire Line
	2350 1250 2450 1250
$Comp
L Device:C_Small C?
U 1 1 5FA4F911
P 2250 1100
AR Path="/5FA4F911" Ref="C?"  Part="1" 
AR Path="/5FA19AFF/5FA4F911" Ref="C26"  Part="1" 
F 0 "C26" H 2350 1150 50  0000 L CNN
F 1 "0.1uF" H 2350 1050 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2288 950 50  0001 C CNN
F 3 "https://media.digikey.com/pdf/Data%20Sheets/Samsung%20PDFs/CL_Series_MLCC_ds.pdf" H 2250 1100 50  0001 C CNN
F 4 "CAP CER 0.1UF 25V X7R 0603" H 2250 1100 50  0001 C CNN "Description"
F 5 "Samsung Electro-Mechanics" H 2250 1100 50  0001 C CNN "Manufacturer"
F 6 "CL10B104KA8NNNC" H 2250 1100 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2250 1100 50  0001 C CNN "Supplier"
F 8 "1276-1006-1-ND" H 2250 1100 50  0001 C CNN "Supplier PN"
	1    2250 1100
	0    1    1    0   
$EndComp
Connection ~ 2350 1100
$Comp
L power:GND #PWR?
U 1 1 5FA4F918
P 2050 1150
AR Path="/5FA4F918" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F918" Ref="#PWR0119"  Part="1" 
F 0 "#PWR0119" H 2050 900 50  0001 C CNN
F 1 "GND" H 2055 977 50  0000 C CNN
F 2 "" H 2050 1150 50  0001 C CNN
F 3 "" H 2050 1150 50  0001 C CNN
	1    2050 1150
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 1100 2050 1100
Wire Wire Line
	2050 1100 2050 1150
Wire Wire Line
	2450 1450 1500 1450
Wire Wire Line
	2450 1650 1750 1650
$Comp
L Device:R_Small R?
U 1 1 5FA4F924
P 1750 1250
AR Path="/5FA4F924" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F924" Ref="R12"  Part="1" 
F 0 "R12" H 1800 1300 50  0000 L CNN
F 1 "2K" H 1800 1200 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 1680 1250 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1750 1250 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 1750 1250 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1750 1250 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 1750 1250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1750 1250 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 1750 1250 50  0001 C CNN "Supplier PN"
	1    1750 1250
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F92A
P 1500 1250
AR Path="/5FA4F92A" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F92A" Ref="R11"  Part="1" 
F 0 "R11" H 1550 1300 50  0000 L CNN
F 1 "2K" H 1550 1200 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 1430 1250 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1500 1250 50  0001 C CNN
F 4 "RES SMD 2K OHM 5% 1/10W 0603" H 1500 1250 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1500 1250 50  0001 C CNN "Manufacturer"
F 6 "CRCW06032K00JNEA" H 1500 1250 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1500 1250 50  0001 C CNN "Supplier"
F 8 "541-2.0KGCT-ND" H 1500 1250 50  0001 C CNN "Supplier PN"
	1    1500 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	1750 1350 1750 1650
Connection ~ 1750 1650
Wire Wire Line
	1750 1650 1250 1650
Wire Wire Line
	1500 1350 1500 1450
Connection ~ 1500 1450
Wire Wire Line
	1500 1450 1250 1450
$Comp
L power:+3.3V #PWR?
U 1 1 5FA4F936
P 1750 1050
AR Path="/5FA4F936" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F936" Ref="#PWR0120"  Part="1" 
F 0 "#PWR0120" H 1750 900 50  0001 C CNN
F 1 "+3.3V" H 1765 1223 50  0000 C CNN
F 2 "" H 1750 1050 50  0001 C CNN
F 3 "" H 1750 1050 50  0001 C CNN
	1    1750 1050
	1    0    0    -1  
$EndComp
Wire Wire Line
	1500 1150 1500 1050
Wire Wire Line
	1500 1050 1750 1050
Wire Wire Line
	1750 1150 1750 1050
Connection ~ 1750 1050
$Comp
L Project:Conn_CUI_MD-60SM J?
U 1 1 5FA4F940
P 6550 1550
AR Path="/5FA4F940" Ref="J?"  Part="1" 
AR Path="/5FA19AFF/5FA4F940" Ref="J5"  Part="1" 
F 0 "J5" H 6550 1965 50  0000 C CNN
F 1 "Mini-DIN 6-pin" H 6550 1874 50  0001 C CNN
F 2 "lib_fp:CUI_MD-60SM" H 6550 950 50  0001 C CNN
F 3 "https://www.cuidevices.com/product/resource/digikeypdf/md-sm-series.pdf" H 6550 1550 50  0001 C CNN
F 4 "Meter Probe" H 6550 1875 50  0000 C CNN "User Label"
F 5 "CONN RCPT FMALE MINI DIN 6P SLDR" H 6550 1550 50  0001 C CNN "Description"
F 6 "CUI Devices" H 6550 1550 50  0001 C CNN "Manufacturer"
F 7 "MD-60SM" H 6550 1550 50  0001 C CNN "Manufacturer PN"
F 8 "Digi-Key" H 6550 1550 50  0001 C CNN "Supplier"
F 9 "CP-2260-ND" H 6550 1550 50  0001 C CNN "Supplier PN"
	1    6550 1550
	1    0    0    -1  
$EndComp
Wire Wire Line
	6600 1900 6500 1900
Connection ~ 6500 1900
Connection ~ 4150 950 
$Comp
L power:+5V #PWR?
U 1 1 5FA4F949
P 4650 950
AR Path="/5FA4F949" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F949" Ref="#PWR0121"  Part="1" 
F 0 "#PWR0121" H 4650 800 50  0001 C CNN
F 1 "+5V" H 4665 1123 50  0000 C CNN
F 2 "" H 4650 950 50  0001 C CNN
F 3 "" H 4650 950 50  0001 C CNN
	1    4650 950 
	1    0    0    -1  
$EndComp
Wire Wire Line
	6050 1150 6050 1550
Wire Wire Line
	6050 1550 6150 1550
Wire Wire Line
	6950 1550 7050 1550
Wire Wire Line
	7050 1550 7050 1900
$Comp
L power:GND #PWR?
U 1 1 5FA4F953
P 7050 1900
AR Path="/5FA4F953" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F953" Ref="#PWR0122"  Part="1" 
F 0 "#PWR0122" H 7050 1650 50  0001 C CNN
F 1 "GND" H 7055 1727 50  0000 C CNN
F 2 "" H 7050 1900 50  0001 C CNN
F 3 "" H 7050 1900 50  0001 C CNN
	1    7050 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3650 1850 4150 1850
Wire Wire Line
	3650 1450 3900 1450
Wire Wire Line
	5800 900  7150 900 
Wire Wire Line
	7150 900  7150 1650
Wire Wire Line
	7150 1650 6950 1650
Connection ~ 3650 1450
Wire Wire Line
	3900 1250 3900 1450
Wire Wire Line
	5850 2150 5850 1000
Wire Wire Line
	5850 1000 7100 1000
Wire Wire Line
	7100 1000 7100 1450
Wire Wire Line
	7100 1450 6950 1450
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5FA4F969
P 3050 2550
AR Path="/5FA4F969" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5FA4F969" Ref="Q1"  Part="1" 
F 0 "Q1" V 3299 2550 50  0000 C CNN
F 1 "BSS138" V 3390 2550 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3250 2475 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3050 2550 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3050 2550 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3050 2550 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3050 2550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3050 2550 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3050 2550 50  0001 C CNN "Supplier PN"
	1    3050 2550
	0    1    1    0   
$EndComp
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5FA4F96F
P 3050 3300
AR Path="/5FA4F96F" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5FA4F96F" Ref="Q2"  Part="1" 
F 0 "Q2" V 3299 3300 50  0000 C CNN
F 1 "BSS138" V 3390 3300 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3250 3225 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3050 3300 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3050 3300 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3050 3300 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3050 3300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3050 3300 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3050 3300 50  0001 C CNN "Supplier PN"
	1    3050 3300
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F975
P 2700 2450
AR Path="/5FA4F975" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F975" Ref="R13"  Part="1" 
F 0 "R13" H 2770 2496 50  0000 L CNN
F 1 "10K" H 2770 2405 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2630 2450 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2700 2450 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2700 2450 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2700 2450 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2700 2450 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2700 2450 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2700 2450 50  0001 C CNN "Supplier PN"
	1    2700 2450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F97B
P 3400 2450
AR Path="/5FA4F97B" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F97B" Ref="R14"  Part="1" 
F 0 "R14" H 3470 2496 50  0000 L CNN
F 1 "10K" H 3470 2405 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3330 2450 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3400 2450 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3400 2450 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3400 2450 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3400 2450 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3400 2450 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3400 2450 50  0001 C CNN "Supplier PN"
	1    3400 2450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F981
P 2700 3200
AR Path="/5FA4F981" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F981" Ref="R15"  Part="1" 
F 0 "R15" H 2770 3246 50  0000 L CNN
F 1 "10K" H 2770 3155 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2630 3200 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2700 3200 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2700 3200 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2700 3200 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2700 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2700 3200 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2700 3200 50  0001 C CNN "Supplier PN"
	1    2700 3200
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F987
P 3400 3200
AR Path="/5FA4F987" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F987" Ref="R16"  Part="1" 
F 0 "R16" H 3470 3246 50  0000 L CNN
F 1 "10K" H 3470 3155 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3330 3200 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3400 3200 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3400 3200 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3400 3200 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3400 3200 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3400 3200 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3400 3200 50  0001 C CNN "Supplier PN"
	1    3400 3200
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA4F98D
P 3400 2300
AR Path="/5FA4F98D" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F98D" Ref="#PWR0123"  Part="1" 
F 0 "#PWR0123" H 3400 2150 50  0001 C CNN
F 1 "+5V" H 3415 2473 50  0000 C CNN
F 2 "" H 3400 2300 50  0001 C CNN
F 3 "" H 3400 2300 50  0001 C CNN
	1    3400 2300
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA4F993
P 2700 2300
AR Path="/5FA4F993" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F993" Ref="#PWR0124"  Part="1" 
F 0 "#PWR0124" H 2700 2150 50  0001 C CNN
F 1 "+3.3V" H 2715 2473 50  0000 C CNN
F 2 "" H 2700 2300 50  0001 C CNN
F 3 "" H 2700 2300 50  0001 C CNN
	1    2700 2300
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA4F999
P 2700 3050
AR Path="/5FA4F999" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F999" Ref="#PWR0125"  Part="1" 
F 0 "#PWR0125" H 2700 2900 50  0001 C CNN
F 1 "+3.3V" H 2715 3223 50  0000 C CNN
F 2 "" H 2700 3050 50  0001 C CNN
F 3 "" H 2700 3050 50  0001 C CNN
	1    2700 3050
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA4F99F
P 3400 3050
AR Path="/5FA4F99F" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F99F" Ref="#PWR0126"  Part="1" 
F 0 "#PWR0126" H 3400 2900 50  0001 C CNN
F 1 "+5V" H 3415 3223 50  0000 C CNN
F 2 "" H 3400 3050 50  0001 C CNN
F 3 "" H 3400 3050 50  0001 C CNN
	1    3400 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	2850 2650 2700 2650
Wire Wire Line
	2700 2650 2700 2550
Wire Wire Line
	3250 2650 3400 2650
Wire Wire Line
	3400 2650 3400 2550
Wire Wire Line
	2700 3300 2700 3400
Wire Wire Line
	2700 3400 2850 3400
Wire Wire Line
	3400 3300 3400 3400
Wire Wire Line
	3400 3400 3250 3400
Wire Wire Line
	3400 3050 3400 3100
Wire Wire Line
	2700 3050 2700 3100
Wire Wire Line
	3400 2300 3400 2350
Wire Wire Line
	2700 2300 2700 2350
Wire Wire Line
	3050 2350 3050 2300
Wire Wire Line
	3050 2300 2700 2300
Connection ~ 2700 2300
Wire Wire Line
	3050 3100 3050 3050
Wire Wire Line
	3050 3050 2700 3050
Connection ~ 2700 3050
Wire Wire Line
	6150 1450 5900 1450
Wire Wire Line
	5900 1450 5900 2650
Wire Wire Line
	5950 2750 5950 1650
Wire Wire Line
	5950 1650 6150 1650
Wire Wire Line
	2500 2650 2700 2650
Connection ~ 2700 2650
Wire Wire Line
	2500 3400 2700 3400
Connection ~ 2700 3400
$Comp
L Power_Protection:USBLC6-2SC6 U?
U 1 1 5FA5B3D0
P 3450 6150
AR Path="/5FA5B3D0" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3D0" Ref="U12"  Part="1" 
F 0 "U12" V 3800 6500 50  0000 R CNN
F 1 "USBLC6-2SC6" V 3100 6000 50  0000 R CNN
F 2 "Package_TO_SOT_SMD:SOT-23-6" H 3450 5650 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/usblc6-2.pdf" H 3650 6500 50  0001 C CNN
F 4 "TVS DIODE 5.25V 17V SOT23-6" H 3450 6150 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 3450 6150 50  0001 C CNN "Manufacturer"
F 6 "USBLC6-2SC6" H 3450 6150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3450 6150 50  0001 C CNN "Supplier"
F 8 "497-5235-1-ND" H 3450 6150 50  0001 C CNN "Supplier PN"
	1    3450 6150
	0    -1   -1   0   
$EndComp
$Comp
L Project:STMPS2141 U?
U 1 1 5FA5B3D6
P 2150 5550
AR Path="/5FA5B3D6" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3D6" Ref="U11"  Part="1" 
F 0 "U11" H 2150 5917 50  0000 C CNN
F 1 "STMPS2141" H 2150 5826 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23-5" H 2150 5900 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/stmps2141.pdf" H 1950 5800 50  0001 C CNN
F 4 "IC PWR SWITCH N-CHAN 1:1 SOT23-5" H 2150 5550 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 2150 5550 50  0001 C CNN "Manufacturer"
F 6 "STMPS2141STR" H 2150 5550 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2150 5550 50  0001 C CNN "Supplier"
F 8 "497-6933-1-ND" H 2150 5550 50  0001 C CNN "Supplier PN"
	1    2150 5550
	1    0    0    -1  
$EndComp
$Comp
L Connector:USB_A J?
U 1 1 5FA5B3DC
P 4850 6150
AR Path="/5FA5B3DC" Ref="J?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3DC" Ref="J7"  Part="1" 
F 0 "J7" H 4900 6600 50  0000 R CNN
F 1 "USB_A" H 4950 6500 50  0000 R CNN
F 2 "Connector_USB:USB_A_Molex_67643_Horizontal" H 5000 6100 50  0001 C CNN
F 3 "https://www.molex.com/pdm_docs/sd/676430910_sd.pdf" H 5000 6100 50  0001 C CNN
F 4 "CONN RCPT TYPEA 4POS R/A" H 4850 6150 50  0001 C CNN "Description"
F 5 "Molex" H 4850 6150 50  0001 C CNN "Manufacturer"
F 6 "0676432910" H 4850 6150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 4850 6150 50  0001 C CNN "Supplier"
F 8 "WM11258-ND" H 4850 6150 50  0001 C CNN "Supplier PN"
	1    4850 6150
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3550 6550 3550 6650
Wire Wire Line
	3550 6650 4300 6650
Wire Wire Line
	4300 6650 4300 6250
Wire Wire Line
	4300 6250 4550 6250
Wire Wire Line
	4300 5650 4300 6150
Wire Wire Line
	4300 6150 4550 6150
Wire Wire Line
	4300 5650 3550 5650
Wire Wire Line
	3550 5650 3550 5750
$Comp
L power:GND #PWR?
U 1 1 5FA5B3EA
P 3850 6200
AR Path="/5FA5B3EA" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3EA" Ref="#PWR0127"  Part="1" 
F 0 "#PWR0127" H 3850 5950 50  0001 C CNN
F 1 "GND" H 3855 6027 50  0000 C CNN
F 2 "" H 3850 6200 50  0001 C CNN
F 3 "" H 3850 6200 50  0001 C CNN
	1    3850 6200
	1    0    0    -1  
$EndComp
Wire Wire Line
	3850 6150 3850 6200
Wire Wire Line
	3050 6150 2850 6150
Wire Wire Line
	2850 6150 2850 6200
$Comp
L power:GND #PWR?
U 1 1 5FA5B3FA
P 2850 6400
AR Path="/5FA5B3FA" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3FA" Ref="#PWR0128"  Part="1" 
F 0 "#PWR0128" H 2850 6150 50  0001 C CNN
F 1 "GND" H 2855 6227 50  0000 C CNN
F 2 "" H 2850 6400 50  0001 C CNN
F 3 "" H 2850 6400 50  0001 C CNN
	1    2850 6400
	1    0    0    -1  
$EndComp
Wire Wire Line
	3050 6150 3050 5550
Wire Wire Line
	3050 5550 4550 5550
Wire Wire Line
	4550 5550 4550 5950
Connection ~ 3050 6150
$Comp
L Device:R_Small R?
U 1 1 5FA5B407
P 2650 5150
AR Path="/5FA5B407" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA5B407" Ref="R19"  Part="1" 
F 0 "R19" H 2720 5196 50  0000 L CNN
F 1 "47K" H 2720 5105 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2580 5150 50  0001 C CNN
F 3 "https://www.vishay.com/docs/28773/crcwce3.pdf" H 2650 5150 50  0001 C CNN
F 4 "RES 47K OHM 5% 1/10W 0603" H 2650 5150 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2650 5150 50  0001 C CNN "Manufacturer"
F 6 "CRCW060347K0JNEAC" H 2650 5150 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2650 5150 50  0001 C CNN "Supplier"
F 8 "541-4061-1-ND" H 2650 5150 50  0001 C CNN "Supplier PN"
	1    2650 5150
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA5B40D
P 2650 5000
AR Path="/5FA5B40D" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B40D" Ref="#PWR0129"  Part="1" 
F 0 "#PWR0129" H 2650 4850 50  0001 C CNN
F 1 "+3.3V" H 2665 5173 50  0000 C CNN
F 2 "" H 2650 5000 50  0001 C CNN
F 3 "" H 2650 5000 50  0001 C CNN
	1    2650 5000
	1    0    0    -1  
$EndComp
Wire Wire Line
	2650 5000 2650 5050
Wire Wire Line
	2650 5250 2650 5650
Wire Wire Line
	2650 5650 2550 5650
$Comp
L Device:R_Small R?
U 1 1 5FA5B416
P 1500 5450
AR Path="/5FA5B416" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA5B416" Ref="R20"  Part="1" 
F 0 "R20" H 1570 5496 50  0000 L CNN
F 1 "10K" H 1570 5405 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 1430 5450 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 1500 5450 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 1500 5450 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 1500 5450 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 1500 5450 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 1500 5450 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 1500 5450 50  0001 C CNN "Supplier PN"
	1    1500 5450
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA5B41C
P 1750 5250
AR Path="/5FA5B41C" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B41C" Ref="#PWR0130"  Part="1" 
F 0 "#PWR0130" H 1750 5100 50  0001 C CNN
F 1 "+5V" H 1765 5423 50  0000 C CNN
F 2 "" H 1750 5250 50  0001 C CNN
F 3 "" H 1750 5250 50  0001 C CNN
	1    1750 5250
	1    0    0    -1  
$EndComp
Wire Wire Line
	1750 5250 1750 5450
Wire Wire Line
	1500 5550 1500 5650
Wire Wire Line
	1500 5650 1750 5650
$Comp
L power:+3.3V #PWR?
U 1 1 5FA5B425
P 1500 5300
AR Path="/5FA5B425" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B425" Ref="#PWR0131"  Part="1" 
F 0 "#PWR0131" H 1500 5150 50  0001 C CNN
F 1 "+3.3V" H 1515 5473 50  0000 C CNN
F 2 "" H 1500 5300 50  0001 C CNN
F 3 "" H 1500 5300 50  0001 C CNN
	1    1500 5300
	1    0    0    -1  
$EndComp
Wire Wire Line
	1500 5300 1500 5350
Wire Wire Line
	1350 5650 1500 5650
Connection ~ 1500 5650
Wire Wire Line
	1350 5900 2650 5900
Wire Wire Line
	2650 5900 2650 5650
Connection ~ 2650 5650
$Comp
L power:GND #PWR?
U 1 1 5FA5B433
P 2150 5950
AR Path="/5FA5B433" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B433" Ref="#PWR0132"  Part="1" 
F 0 "#PWR0132" H 2150 5700 50  0001 C CNN
F 1 "GND" H 2155 5777 50  0000 C CNN
F 2 "" H 2150 5950 50  0001 C CNN
F 3 "" H 2150 5950 50  0001 C CNN
	1    2150 5950
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 5850 2150 5950
$Comp
L power:GND #PWR?
U 1 1 5FA5B43A
P 4850 6650
AR Path="/5FA5B43A" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B43A" Ref="#PWR0133"  Part="1" 
F 0 "#PWR0133" H 4850 6400 50  0001 C CNN
F 1 "GND" H 4855 6477 50  0000 C CNN
F 2 "" H 4850 6650 50  0001 C CNN
F 3 "" H 4850 6650 50  0001 C CNN
	1    4850 6650
	1    0    0    -1  
$EndComp
Wire Wire Line
	4950 6550 4850 6550
Wire Wire Line
	4850 6550 4850 6650
Connection ~ 4850 6550
Text Notes 5550 4950 0    50   ~ 0
Follow datasheet layout guidance for\nall ESD suppression components
Wire Wire Line
	2550 5450 2850 5450
Wire Wire Line
	2850 5450 2850 6150
Connection ~ 2850 6150
Wire Wire Line
	2700 5650 2700 6250
Wire Wire Line
	2700 6250 1350 6250
Wire Wire Line
	3350 6550 3350 6650
Wire Wire Line
	2700 6450 1350 6450
$Comp
L Device:C_Small C27
U 1 1 5F988495
P 2850 6300
F 0 "C27" H 2925 6325 50  0000 L CNN
F 1 "4.7uF" H 2925 6250 50  0000 L CNN
F 2 "Capacitor_SMD:C_0805_2012Metric" H 2850 6300 50  0001 C CNN
F 3 "https://search.murata.co.jp/Ceramy/image/img/A01X/G101/ENG/GRM21BC71H475KE11-01.pdf" H 2850 6300 50  0001 C CNN
F 4 "CAP CER 4.7UF 50V X7S 0805" H 2850 6300 50  0001 C CNN "Description"
F 5 "Murata Electronics" H 2850 6300 50  0001 C CNN "Manufacturer"
F 6 "GRM21BC71H475KE11L" H 2850 6300 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2850 6300 50  0001 C CNN "Supplier"
F 8 "490-12757-1-ND" H 2850 6300 50  0001 C CNN "Supplier PN"
	1    2850 6300
	1    0    0    -1  
$EndComp
Wire Wire Line
	3350 6650 2700 6650
Wire Wire Line
	2700 6650 2700 6450
$Comp
L Connector_Generic:Conn_01x02 J6
U 1 1 5F9A4F1A
P 6350 2900
F 0 "J6" H 6300 3000 50  0000 L CNN
F 1 "Conn_01x02" H 6300 2700 50  0001 L CNN
F 2 "Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical" H 6350 2900 50  0001 C CNN
F 3 "http://www.jst-mfg.com/product/pdf/eng/ePH.pdf" H 6350 2900 50  0001 C CNN
F 4 "Footswitch" H 6300 2700 50  0000 L CNN "User Label"
F 5 "CONN HEADER VERT 2POS 2MM" H 6350 2900 50  0001 C CNN "Description"
F 6 "JST Sales America Inc." H 6350 2900 50  0001 C CNN "Manufacturer"
F 7 "B2B-PH-K-S(LF)(SN)" H 6350 2900 50  0001 C CNN "Manufacturer PN"
F 8 "Digi-Key" H 6350 2900 50  0001 C CNN "Supplier"
F 9 "455-1704-ND" H 6350 2900 50  0001 C CNN "Supplier PN"
	1    6350 2900
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5F9B6C01
P 6050 3100
AR Path="/5F9B6C01" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5F9B6C01" Ref="#PWR071"  Part="1" 
F 0 "#PWR071" H 6050 2850 50  0001 C CNN
F 1 "GND" H 6055 2927 50  0000 C CNN
F 2 "" H 6050 3100 50  0001 C CNN
F 3 "" H 6050 3100 50  0001 C CNN
	1    6050 3100
	1    0    0    -1  
$EndComp
Wire Wire Line
	6150 3000 6050 3000
Wire Wire Line
	6050 3000 6050 3100
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5F9C71D7
P 3050 4050
AR Path="/5F9C71D7" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5F9C71D7" Ref="Q3"  Part="1" 
F 0 "Q3" V 3299 4050 50  0000 C CNN
F 1 "BSS138" V 3390 4050 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3250 3975 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3050 4050 50  0001 L CNN
F 4 "MOSFET N-CH 50V 220MA SOT-23" H 3050 4050 50  0001 C CNN "Description"
F 5 "ON Semiconductor" H 3050 4050 50  0001 C CNN "Manufacturer"
F 6 "BSS138" H 3050 4050 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3050 4050 50  0001 C CNN "Supplier"
F 8 "BSS138CT-ND" H 3050 4050 50  0001 C CNN "Supplier PN"
	1    3050 4050
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5F9C76EB
P 2700 3950
AR Path="/5F9C76EB" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5F9C76EB" Ref="R17"  Part="1" 
F 0 "R17" H 2770 3996 50  0000 L CNN
F 1 "10K" H 2770 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 2630 3950 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 2700 3950 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 2700 3950 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 2700 3950 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 2700 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 2700 3950 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 2700 3950 50  0001 C CNN "Supplier PN"
	1    2700 3950
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5F9C76F5
P 3400 3950
AR Path="/5F9C76F5" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5F9C76F5" Ref="R18"  Part="1" 
F 0 "R18" H 3470 3996 50  0000 L CNN
F 1 "10K" H 3470 3905 50  0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3330 3950 50  0001 C CNN
F 3 "https://www.vishay.com/docs/20035/dcrcwe3.pdf" H 3400 3950 50  0001 C CNN
F 4 "RES SMD 10K OHM 5% 1/10W 0603" H 3400 3950 50  0001 C CNN "Description"
F 5 "Vishay Dale" H 3400 3950 50  0001 C CNN "Manufacturer"
F 6 "CRCW060310K0JNEA" H 3400 3950 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 3400 3950 50  0001 C CNN "Supplier"
F 8 "541-10KGCT-ND" H 3400 3950 50  0001 C CNN "Supplier PN"
	1    3400 3950
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5F9C76FF
P 2700 3800
AR Path="/5F9C76FF" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5F9C76FF" Ref="#PWR073"  Part="1" 
F 0 "#PWR073" H 2700 3650 50  0001 C CNN
F 1 "+3.3V" H 2715 3973 50  0000 C CNN
F 2 "" H 2700 3800 50  0001 C CNN
F 3 "" H 2700 3800 50  0001 C CNN
	1    2700 3800
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5F9C7709
P 3400 3800
AR Path="/5F9C7709" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5F9C7709" Ref="#PWR074"  Part="1" 
F 0 "#PWR074" H 3400 3650 50  0001 C CNN
F 1 "+5V" H 3415 3973 50  0000 C CNN
F 2 "" H 3400 3800 50  0001 C CNN
F 3 "" H 3400 3800 50  0001 C CNN
	1    3400 3800
	1    0    0    -1  
$EndComp
Wire Wire Line
	2700 4050 2700 4150
Wire Wire Line
	2700 4150 2850 4150
Wire Wire Line
	3400 4050 3400 4150
Wire Wire Line
	3400 4150 3250 4150
Wire Wire Line
	3400 3800 3400 3850
Wire Wire Line
	2700 3800 2700 3850
Wire Wire Line
	3050 3850 3050 3800
Wire Wire Line
	3050 3800 2700 3800
Connection ~ 2700 3800
Wire Wire Line
	2500 4150 2700 4150
Connection ~ 2700 4150
$Comp
L Power_Protection:USBLC6-2SC6 U?
U 1 1 5F9E0AB3
P 5050 1700
AR Path="/5F9E0AB3" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5F9E0AB3" Ref="U10"  Part="1" 
F 0 "U10" V 4700 2050 50  0000 R CNN
F 1 "USBLC6-2SC6" V 5400 1550 50  0000 R CNN
F 2 "Package_TO_SOT_SMD:SOT-23-6" H 5050 1200 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/usblc6-2.pdf" H 5250 2050 50  0001 C CNN
F 4 "TVS DIODE 5.25V 17V SOT23-6" H 5050 1700 50  0001 C CNN "Description"
F 5 "STMicroelectronics" H 5050 1700 50  0001 C CNN "Manufacturer"
F 6 "USBLC6-2SC6" H 5050 1700 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5050 1700 50  0001 C CNN "Supplier"
F 8 "497-5235-1-ND" H 5050 1700 50  0001 C CNN "Supplier PN"
	1    5050 1700
	0    -1   1    0   
$EndComp
Wire Wire Line
	5800 1250 5800 900 
Wire Wire Line
	5800 1250 5150 1250
Wire Wire Line
	5150 1250 5150 1300
Wire Wire Line
	4950 1300 4950 1250
Wire Wire Line
	4950 1250 4450 1250
Wire Wire Line
	4450 1250 4450 1450
Wire Wire Line
	4450 1450 3900 1450
Connection ~ 3900 1450
Wire Wire Line
	5150 2100 5150 2150
Wire Wire Line
	5150 2150 5850 2150
Wire Wire Line
	4950 2100 4950 2150
Wire Wire Line
	4950 2150 4400 2150
Wire Wire Line
	4400 2150 4400 1850
Wire Wire Line
	4400 1850 4150 1850
Connection ~ 4150 1850
$Comp
L power:GND #PWR?
U 1 1 5FA27F45
P 5450 1750
AR Path="/5FA27F45" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA27F45" Ref="#PWR070"  Part="1" 
F 0 "#PWR070" H 5450 1500 50  0001 C CNN
F 1 "GND" H 5455 1577 50  0000 C CNN
F 2 "" H 5450 1750 50  0001 C CNN
F 3 "" H 5450 1750 50  0001 C CNN
	1    5450 1750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5450 1700 5450 1750
Wire Wire Line
	6050 1150 4650 1150
Wire Wire Line
	4650 1150 4650 1700
Wire Wire Line
	4650 950  4650 1150
Connection ~ 4650 1150
Wire Wire Line
	3400 2650 4900 2650
Connection ~ 3400 2650
Wire Wire Line
	5950 2750 5000 2750
Wire Wire Line
	3700 2750 3700 3400
Wire Wire Line
	3700 3400 3400 3400
Connection ~ 3400 3400
Wire Wire Line
	6150 2900 5100 2900
Wire Wire Line
	3850 2900 3850 4150
Wire Wire Line
	3850 4150 3400 4150
Connection ~ 3400 4150
Wire Wire Line
	4900 3100 4900 2650
Connection ~ 4900 2650
Wire Wire Line
	4900 2650 5900 2650
Wire Wire Line
	5000 3100 5000 2750
Connection ~ 5000 2750
Wire Wire Line
	5000 2750 3700 2750
Wire Wire Line
	5100 3100 5100 2900
Connection ~ 5100 2900
Wire Wire Line
	5100 2900 3850 2900
NoConn ~ 5200 3100
$Comp
L power:GND #PWR?
U 1 1 5FA61E9F
P 5100 3750
AR Path="/5FA61E9F" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA61E9F" Ref="#PWR072"  Part="1" 
F 0 "#PWR072" H 5100 3500 50  0001 C CNN
F 1 "GND" H 5105 3577 50  0000 C CNN
F 2 "" H 5100 3750 50  0001 C CNN
F 3 "" H 5100 3750 50  0001 C CNN
	1    5100 3750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5100 3700 5100 3750
Text HLabel 1250 1450 0    50   BiDi ~ 0
EXT_I2C_SDA
Text HLabel 1250 1650 0    50   BiDi ~ 0
EXT_I2C_SCL
Text HLabel 2500 2650 0    50   Output ~ 0
SENSOR_BTN
Text HLabel 2500 3400 0    50   Output ~ 0
SENSOR_INT
Text HLabel 2500 4150 0    50   Output ~ 0
FOOTSWITCH
Text HLabel 1350 5650 0    50   Input ~ 0
USB_DRIVE_VBUS
Text HLabel 1350 5900 0    50   Output ~ 0
USB_VBUS_OC
Text HLabel 1350 6250 0    50   BiDi ~ 0
USB_D+
Text HLabel 1350 6450 0    50   BiDi ~ 0
USB_D-
Text Notes 7400 1150 0    50   ~ 0
Meter Pinout
Text Notes 7500 1300 2    50   ~ 0
1
Text Notes 7500 1400 2    50   ~ 0
2
Text Notes 7500 1500 2    50   ~ 0
3
Text Notes 7500 1600 2    50   ~ 0
4
Text Notes 7500 1700 2    50   ~ 0
5
Text Notes 7500 1800 2    50   ~ 0
6
Text Notes 7600 1300 0    50   ~ 0
SDA
Text Notes 7600 1400 0    50   ~ 0
~INT
Text Notes 7600 1500 0    50   ~ 0
GND
Text Notes 7600 1600 0    50   ~ 0
+5V
Text Notes 7600 1700 0    50   ~ 0
SCL
Text Notes 7600 1800 0    50   ~ 0
~BUTTON
Wire Notes Line style solid
	7350 1050 7350 1850
Wire Notes Line style solid
	7350 1850 7950 1850
Wire Notes Line style solid
	7950 1850 7950 1050
Wire Notes Line style solid
	7950 1050 7350 1050
Wire Notes Line style solid
	7350 1175 7950 1175
Wire Notes Line style solid
	7550 1175 7550 1850
$Comp
L Project:DT2636-04S D13
U 1 1 5FAA8A21
P 5100 3400
F 0 "D13" H 5380 3446 50  0000 L CNN
F 1 "DT2636-04S" H 5380 3355 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-363_SC-70-6" H 5400 3350 50  0001 L CNN
F 3 "https://www.diodes.com/assets/Datasheets/DT2636-04S.pdf" H 5425 3075 50  0001 C CNN
F 4 "TVS DIODE 5.5V 9V SOT363" H 5100 3400 50  0001 C CNN "Description"
F 5 "Diodes Incorporated" H 5100 3400 50  0001 C CNN "Manufacturer"
F 6 "DT2636-04S-7" H 5100 3400 50  0001 C CNN "Manufacturer PN"
F 7 "Digi-Key" H 5100 3400 50  0001 C CNN "Supplier"
F 8 "DT2636-04S-7DICT-ND" H 5100 3400 50  0001 C CNN "Supplier PN"
	1    5100 3400
	1    0    0    -1  
$EndComp
Text Notes 6300 3400 0    50   ~ 0
J6 is a connector that connects to\na panel-mounted 3.5mm jack
Text Notes 700  750  0    50   ~ 0
Meter Probe & Footswitch Input
Wire Notes Line
	600  600  600  4550
Wire Notes Line
	600  4550 8150 4550
Wire Notes Line
	8150 4550 8150 600 
Wire Notes Line
	8150 600  600  600 
Text Notes 700  4850 0    50   ~ 0
USB Host Input
Wire Notes Line
	600  4700 600  6950
Wire Notes Line
	600  6950 5400 6950
Wire Notes Line
	5400 6950 5400 4700
Wire Notes Line
	5400 4700 600  4700
Text Label 3650 5650 0    50   ~ 0
USB_CONN_D+
Text Label 3650 6650 0    50   ~ 0
USB_CONN_D-
Wire Wire Line
	2700 5650 3350 5650
Wire Wire Line
	3350 5650 3350 5750
$EndSCHEMATC
