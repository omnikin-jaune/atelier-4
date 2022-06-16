#!/bin/sh

# 
# Vivado(TM)
# runme.sh: a Vivado-generated Runs Script for UNIX
# Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
# 

if [ -z "$PATH" ]; then
  PATH=/usr/share/Xilinx/Vivado/2020.2/ids_lite/ISE/bin/lin64:/usr/share/Xilinx/Vivado/2020.2/bin
else
  PATH=/usr/share/Xilinx/Vivado/2020.2/ids_lite/ISE/bin/lin64:/usr/share/Xilinx/Vivado/2020.2/bin:$PATH
fi
export PATH

if [ -z "$LD_LIBRARY_PATH" ]; then
  LD_LIBRARY_PATH=
else
  LD_LIBRARY_PATH=:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

HD_PWD='/home/raesangur/github/atelier-4/s4InfoAtelier4/s4InfoAtelier4.runs/bd_0837_vsc_0_synth_1'
cd "$HD_PWD"

HD_LOG=runme.log
/bin/touch $HD_LOG

ISEStep="./ISEWrap.sh"
EAStep()
{
     $ISEStep $HD_LOG "$@" >> $HD_LOG 2>&1
     if [ $? -ne 0 ]
     then
         exit
     fi
}

EAStep vivado -log bd_0837_vsc_0.vds -m64 -product Vivado -mode batch -messageDb vivado.pb -notrace -source bd_0837_vsc_0.tcl