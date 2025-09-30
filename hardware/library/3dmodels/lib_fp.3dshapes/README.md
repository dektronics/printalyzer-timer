# 3D Component Models

## Introduction

The 3D models used by the components in these schematics come from a variety
of sources. Some were created for this project, using KiCad's library
scripts, FreeCAD, or Fusion 360. Source files for those are included where
practical. Many others were downloaded from component manufacturer websites
or other 3rd party sources.

Some manufacturers have a habit of freely distributing 3D models for their
components with attached notes saying that they shall not be redistributed.
These notes also often mention that the models are proprietary, confidential,
for personal use only, or something else that would scare you off from
actually admitting that you used them. As such, these models have not been
included in the project repository. Instead, links for downloading them
are provided.

Other models are made available under unspecified or unclear terms.
When those are included, the source is mentioned. If anyone takes issue,
they can be removed from the repository.

## Model List

The models are listed by base filename. Typical distribution files have
the ".step" extension, while source CAD models may have other extensions.

To include models marked as not redistributable, download them from the
original source and convert using the KiCad StepUp plugin for FreeCAD.
Make sure the original and converted files have the same base name as
listed below.

Due to the incremental nature of project development, this list may include
models for components that are no longer part of the actual board design.

Models that are explicitly excluded from the repository have been added to
this directory's `.gitignore` file.

### AMS\_TCS3472
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:AMS_TCS3472_DFN-6`

### B2B-PH-K-S(LF)(SN)
* Status: Included (CC BY-SA /w Design Exception 1.0)
* Source: SnapEDA <https://www.snapeda.com/parts/B2B-PH-K-S(LF)(SN)/JST%20Sales/view-part/?company=&amp;ref=digikey&amp;welcome=home>

### CUI\_ACZ11BR1E-15FD1
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:RotaryEncoder_CUI_ACZ11BR1E-Switch_Vertical_H15mm`

### CUI\_ACZ11BR2E-20FD1
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:RotaryEncoder_CUI_ACZ11BR2E-Switch_Vertical_H20mm`

### CUI\_MD-60SM
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.cuidevices.com/product/resource/3dmodel/md-60sm>
* Footprint: `lib_fp:CUI_MD-60SM`
* Rotation: X = -90 deg
* Offset: X = -7 mm

### CUI\_PSK-6B-S12
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.cui.com/product/resource/3dmodel/psk-6b-s12>
* Footprint: `lib_fp:Converter_ACDC_CUI_PSK-6B`
* Rotation: X = -90 deg
* Offset: X = 22.85 mm, Y = -10.15 mm

### FTSH-107-01-L-DV-K-A
* Status: Included ([Terms](https://www.cadenas.de/terms-of-use-3d-cad-models)
  somewhat unclear, but may be okay for project use.)
* Source: Manufacturer <https://www.samtec.com/partnumber/ftsh-107-01-l-dv-k-a>
* Footprint: `lib_fp:Samtec_FTSH-107-01-L-DV-K-A_2x07_P1.27mm`
* Rotation: X = -90 deg

### G2RL-1A
* Status: Included (No terms listed with download)
* Source: Manufacturer <http://download.ia.omron.com/download/page/G2RL_1A/ECB>
* Footprint: `lib_fp:Relay_SPST_Omron_G2RL-1A`
* Rotation: X = -90 deg
* Offset: Y = -7.5 mm

### G2RL-1A-E
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://download.ia.omron.com/download/page/G2RL_1A_E/ECB>
* Footprint: `lib_fp:Relay_SPST_Omron_G2RL-1A-E`
* Rotation: X = -90 deg
* Offset: Y = -7.5 mm

### Marquardt\_6425\_0101
* Status: Included
* Source: Manufacturer <https://products.marquardt.com/app/portal/products/product/29753;refSeries=28657>
* Note: Converted from JT to STEP using 3rd party tool
* Footprint: `lib_fp:SW_Key_6425_0101`
* Rotation: Z = -90 deg
* Offset: Z = 8.3 mm

### Marquardt\_6425\_1101
* Status: Included
* Source: Manufacturer <https://products.marquardt.com/app/portal/products/product/31097;refSeries=28657>
* Note: Converted from JT to STEP using 3rd party tool
* Footprint: `lib_fp:SW_Key_6425_1101`
* Rotation: Z = -90 deg
* Offset: Z = 8.3 mm

### Marquardt\_6425\_3111
* Status: Included
* Source: Manufacturer <https://products.marquardt.com/app/portal/products/product/29754;refSeries=28657>
* Note: Converted from JT to STEP using 3rd party tool
* Footprint: `lib_fp:SW_Key_6425_0101_LED`
* Rotation: Z = -90 deg
* Offset: Z = 8.3 mm

### Molex\_676432910
* Status: Not included (Not redistributable)
* Source: Manufacturer <https://www.molex.com/molex/products/part-detail/io_connectors/0676432910>

### NHD-3.12-25664UCxx
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.newhavendisplay.com/nhd31225664ucy2-p-3537.html>
* Footprint: `lib_fp:NHD-3.12-25664`
* Rotation: Z = -90 deg
* Offset: X = 24.13 mm, Y = 19.50 mm

### PKM22EPPH4001-B0
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:PKM22EPPH4001-B0`

### PX0580-PC
* Status: Included (No terms listed with download)
* Source: Manufacturer via TraceParts <https://www.traceparts.com/en/product/bulgin-iec-connectors-px0580pc?CatalogPath=TRACEPARTS%3ATP09002002006004&Product=90-08022019-044332&PartNumber=px0580-pc>
* Footprint: `lib_fp:Bulgin_PX0580-PC`
* Rotation: X = -180 deg, Z = 180 deg
* Offset: Y = 6.25 mm

### PX0675-PC
* Status: Included (No terms listed with download)
* Source: Manufacturer via TraceParts <https://www.traceparts.com/en/product/bulgin-iec-outlets-600-series-px0675pc?CatalogPath=BULGIN%3ABULGIN.010.020.230&Product=90-08022019-044408&PartNumber=px0675-pc>
* Footprint: `lib_fp:Bulgin_PX0675-PC`
* Rotation: X = -180 deg, Z = 180 deg
* Offset: Y = 8.75 mm, Z = 16.00 mm

### RLS-126
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://recom-power.com/en/products/dc-dc-converters/rec-p-RLS-126.html>
* Footprint: `lib_fp:L_Recom_RLS-126`
* Rotation: X = -90 deg

### ER-OLEDM032-xx
* Status: Included (Contributed to source by Roman Ty, no terms listed with download)
* Source: 3D ContentCentral <https://www.3dcontentcentral.com/download-model.aspx?catalogid=171&id=559278>
* Footprint: `lib_fp:ER-OLEDM032-1`
* Rotation: Y = -180 deg
* Offset: X = 49.00 mm, Y = 7.38 mm, Z = 3.00 mm

### EastRising\_ER-OLEDM032-1
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.buydisplay.com/yellow-3-2-inch-arduino-raspberry-pi-oled-display-module-256x64-spi>
* Footprint: `lib_fp:Mounting_ER-OLEDM032-1`
* Rotation: X = 180 deg
* Offset: X = -2.9127 mm, Y = 1.2391 mm, Z = 4.00 mm

### Neutrik\_NC5FBH
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.neutrik.com/en/product/nc5fbh>
* Footprint: `Connector_Audio:Jack_XLR_Neutrik_NC5FBH_Horizontal`
* Rotation: X = -90 deg, Z = 90 deg
* Offset: X = 17.15 mm, Y = -3.81 mm, Z = 12.50 mm

### Keystone\_5005
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/5005/Keystone%20Electronics%20Corp./view-part/>
* Footprint: `TestPoint:TestPoint_Keystone_5005-5009_Compact`
* Rotation: X = -90 deg

### Keystone\_5006
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/5006/Keystone%20Electronics%20Corp./view-part/>
* Footprint: `TestPoint:TestPoint_Keystone_5005-5009_Compact`
* Rotation: X = -90 deg

### Keystone\_5007
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/5007/Keystone%20Electronics%20Corp./view-part/>
* Footprint: `TestPoint:TestPoint_Keystone_5005-5009_Compact`
* Rotation: X = -90 deg

### Keystone\_5008
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/5008/Keystone%20Electronics%20Corp./view-part/>
* Footprint: `TestPoint:TestPoint_Keystone_5005-5009_Compact`
* Rotation: X = -90 deg

### Keystone\_5009
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/5009/Keystone%20Electronics%20Corp./view-part/>
* Footprint: `TestPoint:TestPoint_Keystone_5005-5009_Compact`
* Rotation: X = -90 deg

### Keystone\_8879
* Status: Included
* Source: Vendor via SnapMagic <https://www.keyelco.com/product.cfm/product_id/9716>

### SM06B-SRSS-TB-LF
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/SM06B-SRSS-TB(LF)(SN)/JST%20Sales/view-part/>
* Footprint: `Connector_JST:JST_SH_SM06B-SRSS-TB_1x06-1MP_P1.00mm_Horizontal`
* Rotation: -90 deg

### S4B-PH-SM4-TB
* Status: Not included (Not redistributable)
* Source: Manufacturer <https://www.jst-mfg.com/product/index.php?series=199>
* Footprint: `Connector_JST:JST_PH_S4B-PH-SM4-TB_1x04-1MP_P2.00mm_Horizontal`
* Rotation: X = -90 deg
* Offset: X = -11.9 mm, Y = 12.5 mm

### DF22-2P-7.92DSA
* Status: Not included (Not redistributable)
* Source: Manufacturer <https://www.hirose.com/en/product/p/CL0680-1014-8-53>
* Footprint: `lib_fp:Hirose_DF22-2P-7.92DSA_1x02_P7.92mm_Vertical`
* Rotation: Y = 180 deg, Z = 180 deg
* Offset: X = -6.67 mm, Y = -6.67 mm, Z = 20.00 mm

### DF63M-2P-3.96DSA
* Status: Not included (Not redistributable)
* Source: Manufacturer <https://www.hirose.com/product/p/CL0680-0567-0-00>

### TE\_350428-01
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.te.com/usa-en/product-350428-1.html>
* Footprint: `lib_fp:TE_MATE-N-LOK_350428-x_1x02_P6.35mm_Vertical`
* Rotation: Z = 90 deg
* Offset: Z = 14.73 mm

### Cincom\_CFM06Sxxx-E
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.cincon.com/productdetail/CFM06S.html>
* Footprint: `lib_fp:Converter_ACDC_Cincom_CFM06Sxxx-E`
* Rotation: X = 90 deg
* Offset: X = -1.75 mm, Z = 7.10 mm

### Schurter\_6182\_0033.step
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schurter.com/en/info-center/support-tools/cad-drawings?format=STEP&text=6182.0033>
* Footprint: `lib_fp:Schurter_6182_0033`
* Rotation: X = -90 deg, Z = 180 deg
* Offset: Y = 6.2 mm, Z = 16.0 mm

### Schurter\_GSP1\_9113\_1.step
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schurter.com/en/info-center/support-tools/cad-drawings?format=STEP&text=GSP1.9113.1>
* Notes: Original model has been modified to better reflect the specific part variant being used.
* Footprint: `lib_fp:Schurter_GSP1_9113_1`
* Rotation: X = -90 deg, Z = 180 deg
* Offset: Y = 8.70 mm, Z = 15.85 mm

### Schurter\_GSP1\_9101\_1.step
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schurter.com/en/info-center/support-tools/cad-drawings?format=STEP&text=GSP1.9101.1>
* Notes: Original model has been modified to better reflect the specific part variant being used.
* Footprint: `lib_fp:Schurter_GSP1_9101_1`
* Rotation: X = -90 deg, Z = 180 deg
* Offset: Y = 8.70 mm, Z = 15.85 mm

### Molex\_768290002
* Status: Included (Vague terms listed with download)
* Source: Manufacturer <https://www.molex.com/molex/part/partModels.jsp?&prodLevel=part&partNo=768290002&channel=products>
* Footprint: `Connector_Molex:Molex_Mega-Fit_76829-0002_2x01_P5.70mm_Vertical`
* Rotation: X = -180 deg
* Offset: X = 21.376000 mm, Y = -0.466200 mm, Z = 12.654200 mm

### Molex\_1724470002
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.molex.com/en-us/products/part-detail/1724470002>
* Footprint: `lib_fp:Molex_Mini-Fit_Jr_172447_2x01_P4.20mm_Vertical`
* Rotation: X = 90 deg, Z = -180 deg
* Offset: Y = 2.25 mm, Z = 12.80 mm

### L\_XND5050XA
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://ds.yuden.co.jp/TYCOMPAS/ut/detail?pn=LSXND5050XAT3R3MMG&u=M>
* Footprint: `Inductor_SMD:L_Taiyo-Yuden_NR-50xx`

### L\_XNH8080YK
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://ds.yuden.co.jp/TYCOMPAS/ut/detail?pn=LSXNH8080YKL100MJG&u=M>
* Footprint: `Inductor_SMD:L_Taiyo-Yuden_NR-80xx`

### CUI\_MJ-3502
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.cuidevices.com/product/resource/digikey3dmodel/mj-3502>
* Footprint: `lib_fp:Jack_3.5mm_CUI_MJ-3502_Horizontal`
* Rotation: Y = 180 deg, Z = 90 deg
* Offset: Y = 5.0 mm, Z = 6.9 mm

### CNC\_3220-14-0300
* Status: Included (No terms listed with download)
* Source: Manufacturer <http://www.cnctech.us/pdfs/3220-14-0300.zip>
* Notes: Original model has been modified to convert from IGES to STEP and to add colors
* Footprint: `lib_fp:CNC_3220-14-0300-00_2x07_P1.27mm`
* Rotation: X = -90 deg

### B2B-PH-SM4-TB(LF)(SN)
* Status: Included (CC BY-SA /w Design Exception 1.0)
* Source: SnapEDA <https://www.snapeda.com/parts/B2B-PH-SM4-TB(LF)(SN)/JST%20SALES%20AMERICA%20INC%20(VA)/view-part/>
* Footprint: `Connector_JST:JST_PH_B2B-PH-SM4-TB_1x02-1MP_P2.00mm_Vertical`
* Rotation: X = -90 deg
* Offset: Y = 1.75 mm

### B4B-PH-SM4-TB(LF)(SN)
* Status: Included (CC BY-SA /w Design Exception 1.0)
* Source: SnapEDA <https://www.snapeda.com/parts/B4B-PH-SM4-TB%28LF%29%28SN%29/JST%20Sales/view-part/>
* Footprint: `Connector_JST:JST_PH_B4B-PH-SM4-TB_1x04-1MP_P2.00mm_Vertical`
* Rotation: X = -90 deg
* Offset: Y = 1.75 mm

### B5B-PH-SM4-TB(LF)(SN)
* Status: Included (CC BY-SA /w Design Exception 1.0)
* Source: SnapEDA <https://www.snapeda.com/parts/B5B-PH-SM4-TB(LF)(SN)/JST%20Sales/view-part/>
* Footprint: `Connector_JST:JST_PH_B5B-PH-SM4-TB_1x05-1MP_P2.00mm_Vertical`
* Rotation: X = -90 deg
* Offset: Y = 1.75 mm

### BM02B-SRSS-TB
* Status: Not included (Not redistributable)
* Source: Manufacturer <https://www.jst-mfg.com/product/index.php?series=231>

### BM06B-SRSS-TB
* Status: Not included (Not redistributable)
* Source: Manufacturer <https://www.jst-mfg.com/product/index.php?series=231>
* Footprint: `Connector_JST:JST_SH_BM06B-SRSS-TB_1x06-1MP_P1.00mm_Vertical`
* Rotation: X = -90 deg
* Offset: Y = 1.25 mm

### LED\_XLUR11D\_ELM-2-120\_Assembly
* Created as a merged and customized version of two component models
  * LED modified for color and lead length
  * Spacer modified for color and body length
  * Combined to represent component as intended to be used
* XLUR11D
  * Status: Included (No terms listed with download)
  * Source: Manufacturer via link on Digi-Key <https://www.sunledusa.com/products/3d%20file/e/XLxx11.step>
* ELM 2-120
  * Status: Included (No terms listed with download)
  * Source: Manufacturer via link on Digi-Key <https://www.bivar.com/parts_content/Models/ELM%202.IGS>
* Footprint: `lib_fp:LED_D3.0mm_XLUR11D_ELM-2-120`
* Rotation: X = -90 deg
* Offset: X = 1.27 mm

### LED\_LTL-10223W
* Status: Included
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:LED_D4.75mm_LTL-10223W`
* Rotation: Z = 90 deg
* Offset: X = 1.27 mm

### AVX-00-9175-003-001-906
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.kyocera-avx.com/products/connectors/wire-to-board/standard-26-28-awg-00-9175/>
* Footprint: `lib_fp:AVX_IDC-009175-003-001`
* Rotation: X = -90 deg, Z = 90 deg
* Offset: Y = 5 mm

### IRM-10-12
* Status: Included (Contributed to source by Glenn McKie, no terms listed with download)
* Source: 3D ContentCentral <https://www.3dcontentcentral.com/Download-Model.aspx?catalogid=171&id=609491>
* Footprint: `Converter_ACDC:Converter_ACDC_MeanWell_IRM-10-xx_THT`
* Rotation: X = -90 deg, Z = 90 deg
* Offset: X = 19.25 mm, Y = 11.25 mm

### Schurter\_FPG5\_3101\_0050
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schurter.com/en/info-center/support-tools/cad-drawings?format=STEP&text=3101.0050>
* Footprint: `lib_fp:Fuseholder_Cylinder-5x20mm_Schurter_FPG5_3101.0050_Horizontal_Closed`
* Rotation: X = -90 deg, Z = -90 deg
* Offset: X = 24.75 mm

### Amphenol\_87583-3010xx
* Status: Included (No terms listed with download)
* Source: Manufacturer via link on Digi-Key <https://cdn.amphenol-cs.com/media/wysiwyg/files/3d/s87583-3010xx.zip>
* Footprint: `lib_fp:USB_A_Amphenol_87583_Horizontal`
* Rotation: X = -90 deg
* Offset: Y = 6.4 mm

### Amphenol\_74626-11S0BPLF
* Status: Included
* Source: SnapEDA <https://www.snapeda.com/parts/74626-11S0BPLF/Amphenol%20ICC%20(FCI)/view-part/>
* Footprint: `lib_fp:USB_A_Amphenol_74626_Horizontal_Reversed`
* Rotation: X = -90 deg
* Offset: Y = 2.7 mm

### Weidmuller\_1825960000
* Status: Included (No terms listed with download)
* Source: Manufacturer via link on product page <https://catalog.weidmueller.com/catalog/Start.do?ObjectID=1825960000>
* Footprint: `lib_fp:Weidmuller_1825960000_1x02_P5.0mm`
* Offset: X = -4.5500 mm, Y = -4.0500 mm, Z = -3.4999 mm

### AMS\_TSL2585
* Status: Included
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:AMS_TSL2585_OLGA-6_2x1mm_P0.65mm`

### RAC10E-SK\_277
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://recom-power.com/en/products/ac-dc-power-supplies/ac-dc-pcb-mounted/rec-p-RAC10E-12SK!s277.html>
* Footprint: `lib_fp:Converter_ACDC_RECOM_RAC10E-xxSK_277_THT`
* Rotation: X = -180 deg
* Offset: X = -1.75 mm, Y = 9.30 mm, Z = 16.50 mm

### SW-PEC11L-4220F-S0015
* Status: Included
* Source: GrabCAD <https://grabcad.com/library/bourns-pec-encoder-series-1>
* Footprint: `lib_fp:RotaryEncoder_Bourns_PEC11L-Switch_Vertical_H20mm`

### OnSemi\_SC-74-6\_1.5x3.0mm\_P0.95mm
* Status: Included
* Source: Created for project using KiCad packages3D generator and datasheet specs
* Footprint: `lib_fp:OnSemi_SC-74-6_1.5x3.0mm_P0.95mm`

### Schaffner\_RN212-x-02
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schaffner.com/fileadmin/user_upload/pim/products/Schaffner_cad_data_RN_series.zip>
* Footprint: `lib_fp:Choke_Schaffner_RN212-02-12.5x18.0mm`
* Rotation: X = -180 deg, Z = 90 deg
* Offset: X = 5.0 mm, Y = -7.5 mm, Z = 20.0 mm

### Schaffner\_RN218-x-02
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schaffner.com/fileadmin/user_upload/pim/products/Schaffner_cad_data_RN_series.zip>
* Footprint: `lib_fp:Choke_Schaffner_RN212-02-12.5x18.0mm`
* Rotation: X = -180 deg, Z = 90 deg
* Offset: X = 5.0 mm, Y = -6.25 mm, Z = 20.0 mm

### Schaffner\_RN112-x-02
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schaffner.com/fileadmin/user_upload/pim/products/Schaffner_cad_data_RN_series.zip>
* Footprint: `lib_fp:Choke_Schaffner_RN112-02-mm`
* Rotation: X = -180 deg, Z = 90 deg
* Offset: X = 7.5 mm, Y = -5.0 mm, Z = 12.6 mm

### Schaffner\_RN116-x-02
* Status: Included (No terms listed with download)
* Source: Manufacturer <https://www.schaffner.com/fileadmin/user_upload/pim/products/Schaffner_cad_data_RN_series.zip>
* Footprint: `lib_fp:Choke_Schaffner_RN116-02-mm`
* Rotation: X = -180 deg, Z = 90 deg
* Offset: X = 10.05 mm, Y = -6.25 mm, Z = 13.2 mm

### Kemet\_R46KI3100JHM1M
* Status: Included (No terms listed with download)
* Source: Manufacturer via link on Digi-Key <https://www.digikey.com/en/models/13176410>
* Footprint: `lib_fp:C_Rect_L18.0mm_W5.0mm_P15.00mm_Kemet_R46`
* Rotation: X = -90 deg
* Offset: X = -1.65 mm, Y = 2.60 mm

### Kemet\_R46KF310000P1M
* Status: Included (No terms listed with download)
* Source: Manufacturer via link on Digi-Key <https://www.digikey.com/en/models/3724657>
* Footprint: `lib_fp:C_Rect_L13.0mm_W5.0mm_P10.00mm_Kemet_R46`
* Rotation: X = -90 deg
* Offset: X = -1.6 mm, Y = 2.6 mm

### Vishay\_VY1222M47Y5UQ63V0
* Status: Included (No terms listed with download)
* Source: Manufacturer via link on Digi-Key <https://www.digikey.com/en/models/2824500>
* Note: Original model edited to trim lead length
* Footprint: `lib_fp:C_Disc_D12.0mm_W5.0mm_P10.00mm_Vishay_VY1`
* Offset: X = 5.0 mm, Y = -0.8 mm, Z = 10.0 mm

### Vishay\_VY2152M31Y5US63V7
* Status: Included (No terms listed with download)
* Source: Manufacturer via link on Digi-Key <https://www.digikey.com/en/models/6561861>
* Note: Original model edited to trim lead length
* Footprint: `lib_fp:C_Disc_D8.0mm_W5.0mm_P7.50mm_Vishay_VY2`
* Offset: X = 3.75 mm, Y = -0.45 mm, Z = 8.00 mm

### Wurth\_890324023028xx
* Status: Included
* Source: Custom made by combining features of manufacturer STEP and IGES models
  * STEP Model <https://www.we-online.com/components/products/download/Download_STP_890324023028xx%20%28rev1%29.stp>
  * IGES Model <https://www.we-online.com/components/products/download/890324023028%20%28rev1%29.igs>
* Footprint: `lib_fp:C_Rect_L13.0mm_W8.0mm_P10.00mm_Wurth_WCAP-FTX2`
* Offset: X = 5.0 mm

### TI\_PWP0016C
* Status: Included
* Source: Vendor product page (Digi-Key)
* Footprint: `lib_fp:HTSSOP-16-1EP_4.4x5mm_P0.65mm_EP3.4x5mm_Mask2.46x2.31mm_ThermalVias`

### TDK\_VLS6045EX
* Status: Included
* Source: Digi-Key / SnapMagic <https://www.digikey.com/en/models/5286701>
* Footprint: `Inductor_SMD:L_TDK_VLS6045EX_VLS6045AF`
* Rotation: X = -90 deg

### Panasonic\_CP\_Elec\_D
* Status: Included
* Source: Digi-Key / Manufacturer <https://www.digikey.com/en/models/11656944>
* Footprint: `lib_fp:CP_Elec_6.3x6.1`
* Rotation: X = -90 deg, Z = 180 deg

### Harwin\_M20-7810845R
* Status: Included
* Source: TraceParts <https://www.traceparts.com/goto?CatalogPath=TRACEPARTS%3ATP10016001005005&Product=90-19042023-042797&SelectionPath=1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C1%7C>
* Footprint: `lib_fp:Harwin_M20-7810845_2x08_P2.54mm_Vertical_AltNum`
* Rotation: X = -90 deg, X = 90 deg
* Offset: Y = 8.89 mm, Z = 0.20 mm

### SunLike\_LED_S1S0
* Status: Included (Same license as project)
* Source: Created for project using Autodesk Fusion 360
* Footprint: `lib_fp:lib_fp:SunLike_LED_S1S0`

### Omron\_B3F-1052
* Status: Included
* Source: Mouser product page (via Samacsys)
* Footprint: `lib_fp:SW_TH_Tactile_Omron_B3F-10xx`

### Murata\_DLW21HN900SQ2L
* Status: Included
* Source: Vendor via SnapMagic <https://www.murata.com/en-us/products/productdetail?partno=DLW21HN900SQ2%23>
* Footprint: `lib_fp:L_CommonMode_Murata_DLW21HNxxxSQ2x`
* Rotation: X = -90 deg