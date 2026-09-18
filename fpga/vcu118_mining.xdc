## vcu118_mining.xdc -- constraints TEMPLATE for engine bring-up.
## Replace PACKAGE_PINs with your board's clock/reset pins (UG580).

## 200 MHz differential system clock (VCU118 on-board oscillator)
# create_clock -period 5.000 -name sys_clk [get_ports clk_p]
# set_property PACKAGE_PIN <PIN>  [get_ports clk_p]
# set_property PACKAGE_PIN <PIN>  [get_ports clk_n]
# set_property IOSTANDARD DIFF_SSTL12 [get_ports {clk_p clk_n}]

## engine clock from MMCM (200 MHz in, engine domain out)
# create_generated_clock -name eng_clk -source [get_pins mmcm_inst/CLKIN] #     -divide_by 1 [get_pins mmcm_inst/CLKOUT0]

## clock groups: bus clock <-> engine clock are async domains
# set_clock_groups -asynchronous #     -group [get_clocks sys_clk] -group [get_clocks eng_clk]

## template regs are quasi-static (sampled on load pulse): false path to engines
# set_false_path -from [get_cells -hier -filter {NAME =~ *template*}] #     -to [get_clocks eng_clk]

## MARK_DEBUG for the ILA (see ila_mining.tcl)
# set_property MARK_DEBUG true [get_nets -hier {*foundReg*}]
# set_property MARK_DEBUG true [get_nets -hier {*busy*}]
