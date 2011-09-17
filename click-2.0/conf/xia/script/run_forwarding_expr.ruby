#!/usr/bin/env ruby

# This script assumes that you have the following files in both ROTUER and PACKETGEN machines
LOAD_CLICK_CMD = "/home/dongsuh/xia-core/click-2.0/load_user_click.sh"
IP_ROUTER_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_ip_router.sh"
IP_PKT_GEN_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_ip_pktgen.sh"
XIA_ROUTER_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_router.sh" 
XIA_PKT_GEN_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_pktgen.sh"
XIA_PKT_GEN_FB1_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_pktgen_fb1.sh"
XIA_PKT_GEN_FB2_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_pktgen_fb2.sh"
XIA_PKT_GEN_FB3_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_pktgen_fb3.sh"
XIA_PKT_GEN_VIA_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_pktgen_via.sh"
XIA_PKT_GEN_ISO_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/run_xia_pktgen_iso.sh"
RECORD_STAT_SCRIPT = "/home/dongsuh/xia-core/click-2.0/conf/xia/script/record_stat.sh"

RESET_CLICK_CMD = "killall click"

SETUP = [ 
#	{:NAME => "XIA-%d-FB0", :ROUTER =>XIA_ROUTER_SCRIPT, :PKTGEN => XIA_PKT_GEN_SCRIPT, :PKT_OVERHEAD =>98},
#	{:NAME => "XIA-%d-FB3", :ROUTER =>XIA_ROUTER_SCRIPT, :PKTGEN => XIA_PKT_GEN_FB3_SCRIPT, :PKT_OVERHEAD =>182},
#	{:NAME => "XIA-%d-FB2", :ROUTER =>XIA_ROUTER_SCRIPT, :PKTGEN => XIA_PKT_GEN_FB2_SCRIPT, :PKT_OVERHEAD =>154},
#	{:NAME => "XIA-%d-FB1", :ROUTER =>XIA_ROUTER_SCRIPT, :PKTGEN => XIA_PKT_GEN_FB1_SCRIPT, :PKT_OVERHEAD =>126},
#	{:NAME => "IP-%d-NOCP", :ROUTER => IP_ROUTER_SCRIPT, :PKTGEN => IP_PKT_GEN_SCRIPT, :PKT_OVERHEAD =>34},
	{:NAME => "XIA-%d-isolation-%d", :ROUTER => XIA_ROUTER_SCRIPT, :PKTGEN =>XIA_PKT_GEN_ISO_SCRIPT, :PKT_OVERHEAD =>126},
#	{:NAME => "XIA-%d-VIA", :ROUTER =>XIA_ROUTER_SCRIPT, :PKTGEN => XIA_PKT_GEN_VIA_SCRIPT, :PKT_OVERHEAD =>126}
	]
class Flags
  @flag_bit = 0
  public
  class << self
  def set(*syms)
    syms.each { |s| const_set(s, 2**@flag_bit) ;@flag_bit+=1 }
    const_set(:DEFAULT, syms.first) unless syms.nil?
  end
  end
end

Flags.set(:BACKGROUND)

ROUTER="ng2.nan.cs.cmu.edu"
PACKETGEN ="ng3.nan.cs.cmu.edu"
LOCAL = "localhost"

def run_command(machine, cmd, mode = Flags::BACKGROUND)
  if (mode & Flags::BACKGROUND != 0)
    cmd = "-f \"#{cmd.to_s} \""
  else
    cmd = "\"#{cmd.to_s} ; exit\""
  end
  ssh = "ssh #{machine} #{cmd}"
  puts ssh
  system(ssh)
end

def load_click(machine, mode)
  run_command(machine, LOAD_CLICK_CMD, mode)
end

def reset_click(machine, mode)
  run_command(machine, RESET_CLICK_CMD, mode)
  run_command(machine, RESET_CLICK_CMD, mode)
end

def collect_stats(machine, size)
  run_command(machine, "#{RECORD_STAT_SCRIPT} #{size}", 0) 
end

if __FILE__ ==$0
   load_click(ROUTER, 0)
   sleep(1)
   load_click(PACKETGEN, 0)
   sleep(3)

  SETUP.each do |setup|
    overhead = setup[:PKT_OVERHEAD]
    router_script = setup[:ROUTER] 
    pktgen_script = setup[:PKTGEN] 
   
    min_pktsize = (overhead+63)/64  * 64
    size = min_pktsize
    pkt_size = []

    while (size<=256)
 	pkt_size.push(size)	
	size+=64
    end
    while (size<=1500)
 	pkt_size.push(size)	
	size+=128
    end
    pkt_size.push(1500)	

    if (setup[:NAME]=="XIA-%d-isolation-%d")
      pkt_size =  (1..11).to_a
    end
   
    p setup[:NAME]    
    p pkt_size

    pkt_size.each do |size|
        if (setup[:NAME]=="XIA-%d-isolation-%d")
	  isolate = size
          size = 256
          exp_name = setup[:NAME] % [size , isolate]
        else
          exp_name = setup[:NAME] % size
        end
        payload_size = size - overhead

        reset_click(ROUTER, 0)
        sleep(1)
        reset_click(PACKETGEN, 0)
        sleep(3)
        # run router
        run_command(ROUTER, router_script)
        sleep(3)
        # run packet gen
        if (setup[:NAME]=="XIA-%d-isolation-%d")
          run_command(PACKETGEN, "#{pktgen_script} #{payload_size} #{isolate}")
        else
          run_command(PACKETGEN, "#{pktgen_script} #{payload_size}")
        end
        
        sleep(10)
        collect_stats(ROUTER, "#{exp_name}")  
        sleep(3)

    end
  end
end

