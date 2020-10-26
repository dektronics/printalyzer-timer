EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 4 4
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L power:+5V #PWR?
U 1 1 5FA4F8DB
P 4250 950
AR Path="/5FA4F8DB" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8DB" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 4250 800 50  0001 C CNN
F 1 "+5V" H 4265 1123 50  0000 C CNN
F 2 "" H 4250 950 50  0001 C CNN
F 3 "" H 4250 950 50  0001 C CNN
	1    4250 950 
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR?
U 1 1 5FA4F8E1
P 5400 1900
AR Path="/5FA4F8E1" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8E1" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 5400 1650 50  0001 C CNN
F 1 "GND" H 5405 1727 50  0000 C CNN
F 2 "" H 5400 1900 50  0001 C CNN
F 3 "" H 5400 1900 50  0001 C CNN
	1    5400 1900
	1    0    0    -1  
$EndComp
$Comp
L Interface_Expansion:P82B96 U?
U 1 1 5FA4F8E7
P 3150 1550
AR Path="/5FA4F8E7" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8E7" Ref="U?"  Part="1" 
F 0 "U?" H 3150 2167 50  0000 C CNN
F 1 "P82B96" H 3150 2076 50  0000 C CNN
F 2 "" H 3150 1550 50  0001 C CNN
F 3 "http://www.nxp.com/documents/data_sheet/P82B96.pdf" H 3150 1550 50  0001 C CNN
	1    3150 1550
	1    0    0    -1  
$EndComp
Text GLabel 2600 2650 0    50   Input ~ 0
SENSOR_BTN
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
AR Path="/5FA19AFF/5FA4F8F1" Ref="R?"  Part="1" 
F 0 "R?" H 4320 1196 50  0000 L CNN
F 1 "2K" H 4320 1105 50  0000 L CNN
F 2 "" V 4180 1150 50  0001 C CNN
F 3 "~" H 4250 1150 50  0001 C CNN
	1    4250 1150
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F8F7
P 4000 1150
AR Path="/5FA4F8F7" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F8F7" Ref="R?"  Part="1" 
F 0 "R?" H 4070 1196 50  0000 L CNN
F 1 "2K" H 4070 1105 50  0000 L CNN
F 2 "" V 3930 1150 50  0001 C CNN
F 3 "~" H 4000 1150 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F901" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2450 1700 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F909" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2450 950 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F911" Ref="C?"  Part="1" 
F 0 "C?" H 2465 1146 50  0000 L CNN
F 1 "0.1uF" H 2465 1055 50  0000 L CNN
F 2 "" H 2388 950 50  0001 C CNN
F 3 "~" H 2350 1100 50  0001 C CNN
	1    2350 1100
	0    1    1    0   
$EndComp
Connection ~ 2450 1100
$Comp
L power:GND #PWR?
U 1 1 5FA4F918
P 2150 1150
AR Path="/5FA4F918" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F918" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2150 900 50  0001 C CNN
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
Text Label 1350 1450 2    50   ~ 0
EXT_I2C_SDA
Text Label 1350 1650 2    50   ~ 0
EXT_I2C_SCA
Wire Wire Line
	2550 1450 1600 1450
Wire Wire Line
	2550 1650 1850 1650
$Comp
L Device:R_Small R?
U 1 1 5FA4F924
P 1850 1250
AR Path="/5FA4F924" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F924" Ref="R?"  Part="1" 
F 0 "R?" H 1920 1296 50  0000 L CNN
F 1 "2K" H 1920 1205 50  0000 L CNN
F 2 "" V 1780 1250 50  0001 C CNN
F 3 "~" H 1850 1250 50  0001 C CNN
	1    1850 1250
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F92A
P 1600 1250
AR Path="/5FA4F92A" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F92A" Ref="R?"  Part="1" 
F 0 "R?" H 1670 1296 50  0000 L CNN
F 1 "2K" H 1670 1205 50  0000 L CNN
F 2 "" V 1530 1250 50  0001 C CNN
F 3 "~" H 1600 1250 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F936" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 1850 900 50  0001 C CNN
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
P 5450 1550
AR Path="/5FA4F940" Ref="J?"  Part="1" 
AR Path="/5FA19AFF/5FA4F940" Ref="J?"  Part="1" 
F 0 "J?" H 5450 1965 50  0000 C CNN
F 1 "Meter Probe" H 5450 1874 50  0000 C CNN
F 2 "lib_fp:CUI_MD-60SM" H 5450 950 50  0001 C CNN
F 3 "https://www.cuidevices.com/product/resource/digikeypdf/md-sm-series.pdf" H 5450 1550 50  0001 C CNN
	1    5450 1550
	1    0    0    -1  
$EndComp
Wire Wire Line
	5500 1900 5400 1900
Connection ~ 5400 1900
Connection ~ 4250 950 
$Comp
L power:+5V #PWR?
U 1 1 5FA4F949
P 4950 1250
AR Path="/5FA4F949" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F949" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 4950 1100 50  0001 C CNN
F 1 "+5V" H 4965 1423 50  0000 C CNN
F 2 "" H 4950 1250 50  0001 C CNN
F 3 "" H 4950 1250 50  0001 C CNN
	1    4950 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	4950 1250 4950 1550
Wire Wire Line
	4950 1550 5050 1550
Wire Wire Line
	5850 1550 5950 1550
Wire Wire Line
	5950 1550 5950 1900
$Comp
L power:GND #PWR?
U 1 1 5FA4F953
P 5950 1900
AR Path="/5FA4F953" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F953" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 5950 1650 50  0001 C CNN
F 1 "GND" H 5955 1727 50  0000 C CNN
F 2 "" H 5950 1900 50  0001 C CNN
F 3 "" H 5950 1900 50  0001 C CNN
	1    5950 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3750 1850 4250 1850
Connection ~ 4250 1850
Wire Wire Line
	4250 1850 4750 1850
Wire Wire Line
	3750 1450 4000 1450
Wire Wire Line
	4700 1450 4700 900 
Wire Wire Line
	4700 900  6050 900 
Wire Wire Line
	6050 900  6050 1650
Wire Wire Line
	6050 1650 5850 1650
Connection ~ 3750 1450
Wire Wire Line
	4000 1250 4000 1450
Connection ~ 4000 1450
Wire Wire Line
	4000 1450 4700 1450
Wire Wire Line
	4750 1850 4750 1000
Wire Wire Line
	4750 1000 6000 1000
Wire Wire Line
	6000 1000 6000 1450
Wire Wire Line
	6000 1450 5850 1450
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5FA4F969
P 3150 2550
AR Path="/5FA4F969" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5FA4F969" Ref="Q?"  Part="1" 
F 0 "Q?" V 3399 2550 50  0000 C CNN
F 1 "BSS138" V 3490 2550 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3350 2475 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3150 2550 50  0001 L CNN
	1    3150 2550
	0    1    1    0   
$EndComp
$Comp
L Transistor_FET:BSS138 Q?
U 1 1 5FA4F96F
P 3150 3300
AR Path="/5FA4F96F" Ref="Q?"  Part="1" 
AR Path="/5FA19AFF/5FA4F96F" Ref="Q?"  Part="1" 
F 0 "Q?" V 3399 3300 50  0000 C CNN
F 1 "BSS138" V 3490 3300 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23" H 3350 3225 50  0001 L CIN
F 3 "https://www.onsemi.com/pub/Collateral/BSS138-D.PDF" H 3150 3300 50  0001 L CNN
	1    3150 3300
	0    1    1    0   
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F975
P 2800 2450
AR Path="/5FA4F975" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F975" Ref="R?"  Part="1" 
F 0 "R?" H 2870 2496 50  0000 L CNN
F 1 "10K" H 2870 2405 50  0000 L CNN
F 2 "" V 2730 2450 50  0001 C CNN
F 3 "~" H 2800 2450 50  0001 C CNN
	1    2800 2450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F97B
P 3500 2450
AR Path="/5FA4F97B" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F97B" Ref="R?"  Part="1" 
F 0 "R?" H 3570 2496 50  0000 L CNN
F 1 "10K" H 3570 2405 50  0000 L CNN
F 2 "" V 3430 2450 50  0001 C CNN
F 3 "~" H 3500 2450 50  0001 C CNN
	1    3500 2450
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F981
P 2800 3200
AR Path="/5FA4F981" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F981" Ref="R?"  Part="1" 
F 0 "R?" H 2870 3246 50  0000 L CNN
F 1 "10K" H 2870 3155 50  0000 L CNN
F 2 "" V 2730 3200 50  0001 C CNN
F 3 "~" H 2800 3200 50  0001 C CNN
	1    2800 3200
	1    0    0    -1  
$EndComp
$Comp
L Device:R_Small R?
U 1 1 5FA4F987
P 3500 3200
AR Path="/5FA4F987" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA4F987" Ref="R?"  Part="1" 
F 0 "R?" H 3570 3246 50  0000 L CNN
F 1 "10K" H 3570 3155 50  0000 L CNN
F 2 "" V 3430 3200 50  0001 C CNN
F 3 "~" H 3500 3200 50  0001 C CNN
	1    3500 3200
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA4F98D
P 3500 2300
AR Path="/5FA4F98D" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA4F98D" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 3500 2150 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F993" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2800 2150 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F999" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2800 2900 50  0001 C CNN
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
AR Path="/5FA19AFF/5FA4F99F" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 3500 2900 50  0001 C CNN
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
	5050 1450 4800 1450
Wire Wire Line
	4800 1450 4800 2650
Wire Wire Line
	4800 2650 3500 2650
Connection ~ 3500 2650
Wire Wire Line
	3500 3400 4850 3400
Wire Wire Line
	4850 3400 4850 1650
Wire Wire Line
	4850 1650 5050 1650
Connection ~ 3500 3400
Text GLabel 2600 3400 0    50   Input ~ 0
SENSOR_INT
Wire Wire Line
	2600 2650 2800 2650
Connection ~ 2800 2650
Wire Wire Line
	2600 3400 2800 3400
Connection ~ 2800 3400
Text Notes 4950 2500 0    50   Italic 0
Add ESD protection on all connector pins\n\nThe USBLC6 part used for the USB port may work fine for this,\nas there's a recommended Semtech part that looks almost identical.
$Comp
L Power_Protection:USBLC6-2SC6 U?
U 1 1 5FA5B3D0
P 3850 5800
AR Path="/5FA5B3D0" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3D0" Ref="U?"  Part="1" 
F 0 "U?" V 4200 5550 50  0000 R CNN
F 1 "USBLC6-2SC6" V 3500 5650 50  0000 R CNN
F 2 "Package_TO_SOT_SMD:SOT-23-6" H 3850 5300 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/usblc6-2.pdf" H 4050 6150 50  0001 C CNN
	1    3850 5800
	0    -1   -1   0   
$EndComp
$Comp
L Project:STMPS2141 U?
U 1 1 5FA5B3D6
P 2550 5200
AR Path="/5FA5B3D6" Ref="U?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3D6" Ref="U?"  Part="1" 
F 0 "U?" H 2550 5567 50  0000 C CNN
F 1 "STMPS2141" H 2550 5476 50  0000 C CNN
F 2 "Package_TO_SOT_SMD:SOT-23-5" H 2550 5550 50  0001 C CNN
F 3 "https://www.st.com/resource/en/datasheet/stmps2141.pdf" H 2350 5450 50  0001 C CNN
	1    2550 5200
	1    0    0    -1  
$EndComp
$Comp
L Connector:USB_A J?
U 1 1 5FA5B3DC
P 5250 5800
AR Path="/5FA5B3DC" Ref="J?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3DC" Ref="J?"  Part="1" 
F 0 "J?" H 5020 5789 50  0000 R CNN
F 1 "USB_A" H 5020 5698 50  0000 R CNN
F 2 "" H 5400 5750 50  0001 C CNN
F 3 " ~" H 5400 5750 50  0001 C CNN
	1    5250 5800
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3950 6200 3950 6300
Wire Wire Line
	3950 6300 4700 6300
Wire Wire Line
	4700 6300 4700 5900
Wire Wire Line
	4700 5900 4950 5900
Wire Wire Line
	4700 5300 4700 5800
Wire Wire Line
	4700 5800 4950 5800
Wire Wire Line
	4700 5300 3950 5300
Wire Wire Line
	3950 5300 3950 5400
$Comp
L power:GND #PWR?
U 1 1 5FA5B3EA
P 4250 5850
AR Path="/5FA5B3EA" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3EA" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 4250 5600 50  0001 C CNN
F 1 "GND" H 4255 5677 50  0000 C CNN
F 2 "" H 4250 5850 50  0001 C CNN
F 3 "" H 4250 5850 50  0001 C CNN
	1    4250 5850
	1    0    0    -1  
$EndComp
Wire Wire Line
	4250 5800 4250 5850
Wire Wire Line
	3450 5800 3250 5800
Wire Wire Line
	3250 5800 3250 5850
$Comp
L power:GND #PWR?
U 1 1 5FA5B3FA
P 3250 6050
AR Path="/5FA5B3FA" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B3FA" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 3250 5800 50  0001 C CNN
F 1 "GND" H 3255 5877 50  0000 C CNN
F 2 "" H 3250 6050 50  0001 C CNN
F 3 "" H 3250 6050 50  0001 C CNN
	1    3250 6050
	1    0    0    -1  
$EndComp
Wire Wire Line
	3450 5800 3450 5200
Wire Wire Line
	3450 5200 4950 5200
Wire Wire Line
	4950 5200 4950 5600
Connection ~ 3450 5800
$Comp
L Device:R_Small R?
U 1 1 5FA5B407
P 3050 4800
AR Path="/5FA5B407" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA5B407" Ref="R?"  Part="1" 
F 0 "R?" H 3120 4846 50  0000 L CNN
F 1 "47K" H 3120 4755 50  0000 L CNN
F 2 "" V 2980 4800 50  0001 C CNN
F 3 "~" H 3050 4800 50  0001 C CNN
	1    3050 4800
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR?
U 1 1 5FA5B40D
P 3050 4650
AR Path="/5FA5B40D" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B40D" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 3050 4500 50  0001 C CNN
F 1 "+3.3V" H 3065 4823 50  0000 C CNN
F 2 "" H 3050 4650 50  0001 C CNN
F 3 "" H 3050 4650 50  0001 C CNN
	1    3050 4650
	1    0    0    -1  
$EndComp
Wire Wire Line
	3050 4650 3050 4700
Wire Wire Line
	3050 4900 3050 5300
Wire Wire Line
	3050 5300 2950 5300
$Comp
L Device:R_Small R?
U 1 1 5FA5B416
P 1900 5100
AR Path="/5FA5B416" Ref="R?"  Part="1" 
AR Path="/5FA19AFF/5FA5B416" Ref="R?"  Part="1" 
F 0 "R?" H 1970 5146 50  0000 L CNN
F 1 "10K" H 1970 5055 50  0000 L CNN
F 2 "" V 1830 5100 50  0001 C CNN
F 3 "~" H 1900 5100 50  0001 C CNN
	1    1900 5100
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR?
U 1 1 5FA5B41C
P 2150 4900
AR Path="/5FA5B41C" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B41C" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2150 4750 50  0001 C CNN
F 1 "+5V" H 2165 5073 50  0000 C CNN
F 2 "" H 2150 4900 50  0001 C CNN
F 3 "" H 2150 4900 50  0001 C CNN
	1    2150 4900
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 4900 2150 5100
Wire Wire Line
	1900 5200 1900 5300
Wire Wire Line
	1900 5300 2150 5300
$Comp
L power:+3.3V #PWR?
U 1 1 5FA5B425
P 1900 4950
AR Path="/5FA5B425" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B425" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 1900 4800 50  0001 C CNN
F 1 "+3.3V" H 1915 5123 50  0000 C CNN
F 2 "" H 1900 4950 50  0001 C CNN
F 3 "" H 1900 4950 50  0001 C CNN
	1    1900 4950
	1    0    0    -1  
$EndComp
Wire Wire Line
	1900 4950 1900 5000
Text GLabel 1750 5300 0    50   Input ~ 0
USB_DRIVE_VBUS
Text GLabel 1750 5550 0    50   Input ~ 0
USB_VBUS_OC
Wire Wire Line
	1750 5300 1900 5300
Connection ~ 1900 5300
Wire Wire Line
	1750 5550 3050 5550
Wire Wire Line
	3050 5550 3050 5300
Connection ~ 3050 5300
$Comp
L power:GND #PWR?
U 1 1 5FA5B433
P 2550 5600
AR Path="/5FA5B433" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B433" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 2550 5350 50  0001 C CNN
F 1 "GND" H 2555 5427 50  0000 C CNN
F 2 "" H 2550 5600 50  0001 C CNN
F 3 "" H 2550 5600 50  0001 C CNN
	1    2550 5600
	1    0    0    -1  
$EndComp
Wire Wire Line
	2550 5500 2550 5600
$Comp
L power:GND #PWR?
U 1 1 5FA5B43A
P 5250 6300
AR Path="/5FA5B43A" Ref="#PWR?"  Part="1" 
AR Path="/5FA19AFF/5FA5B43A" Ref="#PWR?"  Part="1" 
F 0 "#PWR?" H 5250 6050 50  0001 C CNN
F 1 "GND" H 5255 6127 50  0000 C CNN
F 2 "" H 5250 6300 50  0001 C CNN
F 3 "" H 5250 6300 50  0001 C CNN
	1    5250 6300
	1    0    0    -1  
$EndComp
Wire Wire Line
	5350 6200 5250 6200
Wire Wire Line
	5250 6200 5250 6300
Connection ~ 5250 6200
Text Notes 3650 6500 0    50   Italic 0
Follow layout guidance in datasheet
Text GLabel 2800 5900 0    50   Input ~ 0
USB_DP
Text GLabel 2800 6100 0    50   Input ~ 0
USB_DM
Wire Wire Line
	2950 5100 3250 5100
Wire Wire Line
	3250 5100 3250 5800
Connection ~ 3250 5800
Wire Wire Line
	3750 5400 3100 5400
Wire Wire Line
	3100 5400 3100 5900
Wire Wire Line
	3100 5900 2800 5900
Wire Wire Line
	3750 6200 3750 6300
Wire Wire Line
	3100 6100 2800 6100
$Comp
L Device:C_Small C?
U 1 1 5F988495
P 3250 5950
F 0 "C?" H 3342 5996 50  0000 L CNN
F 1 "0.1uF" H 3342 5905 50  0000 L CNN
F 2 "" H 3250 5950 50  0001 C CNN
F 3 "~" H 3250 5950 50  0001 C CNN
	1    3250 5950
	1    0    0    -1  
$EndComp
Wire Wire Line
	3750 6300 3100 6300
Wire Wire Line
	3100 6300 3100 6100
Text Notes 7850 3800 0    50   Italic 0
Add barrel jack for footswitch
Wire Notes Line
	7750 3650 7750 3900
Wire Notes Line
	7750 3900 9100 3900
Wire Notes Line
	9100 3900 9100 3650
Wire Notes Line
	9100 3650 7750 3650
$EndSCHEMATC