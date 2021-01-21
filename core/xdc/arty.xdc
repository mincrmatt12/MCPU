# Clock pin
set_property PACKAGE_PIN E3 [get_ports {clk}]
set_property IOSTANDARD LVCMOS33 [get_ports {clk}]

# 7seg
set_property PACKAGE_PIN T10 [get_ports {cathodes[0]}]
set_property PACKAGE_PIN R10 [get_ports {cathodes[1]}] #IO_25_14 Sch=cb
set_property PACKAGE_PIN K16 [get_ports {cathodes[2]}] #IO_25_15 Sch=cc
set_property PACKAGE_PIN K13 [get_ports {cathodes[3]}] #IO_L17P_T2_A26_15 Sch=cd
set_property PACKAGE_PIN P15 [get_ports {cathodes[4]}] #IO_L13P_T2_MRCC_14 Sch=ce
set_property PACKAGE_PIN T11 [get_ports {cathodes[5]}] #IO_L19P_T3_A10_D26_14 Sch=cf
set_property PACKAGE_PIN L18 [get_ports {cathodes[6]}] #IO_L4P_T0_D04_14 Sch=cg
set_property PACKAGE_PIN H15 [get_ports {cathodes[7]}] #IO_L19N_T3_A21_VREF_15 Sch=dp
set_property PACKAGE_PIN J17 [get_ports {anodes[0]}] #IO_L23P_T3_FOE_B_15 Sch=an[0]
set_property PACKAGE_PIN J18 [get_ports {anodes[1]}] #IO_L23N_T3_FWE_B_15 Sch=an[1]
set_property PACKAGE_PIN T9  [get_ports {anodes[2]}] #IO_L24P_T3_A01_D17_14 Sch=an[2]
set_property PACKAGE_PIN J14 [get_ports {anodes[3]}] #IO_L19P_T3_A22_15 Sch=an[3]
set_property PACKAGE_PIN P14 [get_ports {anodes[4]}] #IO_L8N_T1_D12_14 Sch=an[4]
set_property PACKAGE_PIN T14 [get_ports {anodes[5]}] #IO_L14P_T2_SRCC_14 Sch=an[5]
set_property PACKAGE_PIN K2  [get_ports {anodes[6]}] #IO_L23P_T3_35 Sch=an[6]
set_property PACKAGE_PIN U13 [get_ports {anodes[7]}] #IO_L23N_T3_A02_D18_14 Sch=an[7]

set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[0]}] #IO_L24N_T3_A00_D16_14 Sch=ca
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[1]}] #IO_25_14 Sch=cb
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[2]}] #IO_25_15 Sch=cc
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[3]}] #IO_L17P_T2_A26_15 Sch=cd
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[4]}] #IO_L13P_T2_MRCC_14 Sch=ce
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[5]}] #IO_L19P_T3_A10_D26_14 Sch=cf
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[6]}] #IO_L4P_T0_D04_14 Sch=cg
set_property IOSTANDARD LVCMOS33 [get_ports {cathodes[7]}] #IO_L19N_T3_A21_VREF_15 Sch=dp
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[0]}] #IO_L23P_T3_FOE_B_15 Sch=an[0]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[1]}] #IO_L23N_T3_FWE_B_15 Sch=an[1]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[2]}] #IO_L24P_T3_A01_D17_14 Sch=an[2]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[3]}] #IO_L19P_T3_A22_15 Sch=an[3]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[4]}] #IO_L8N_T1_D12_14 Sch=an[4]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[5]}] #IO_L14P_T2_SRCC_14 Sch=an[5]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[6]}] #IO_L23P_T3_35 Sch=an[6]
set_property IOSTANDARD LVCMOS33 [get_ports {anodes[7]}] #IO_L23N_T3_A02_D18_14 Sch=an[7]

# Clock constraints
create_clock -period 10.0 [get_ports {clk}]
