----------------------------------------------------------------------------------
--
-- The MIT License (MIT)
--
-- Copyright (c) 2016 Michael J. Wouters
-- 
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
-- 
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
-- 
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_arith.all;
use IEEE.std_logic_misc.all;
use IEEE.std_logic_unsigned.all;

library UNISIM;
use UNISIM.vcomponents.all;

use work.FRONTPANEL.all;
use work.TRIGGERS.OneShot;
use work.TICOUNTERS.TICounter32;

entity TTSCounterPPSCR is
	port (
		hi_in     : in STD_LOGIC_VECTOR(7 downto 0);
		hi_out    : out STD_LOGIC_VECTOR(1 downto 0);
		hi_inout  : inout STD_LOGIC_VECTOR(15 downto 0);
		hi_muxsel : out STD_LOGIC;
		
		ct1b  : in STD_LOGIC;
		ct2b  : in STD_LOGIC;
		ct3b  : in STD_LOGIC;
		ct4b  : in STD_LOGIC;
		ct5b  : in STD_LOGIC;
		refpps: in STD_LOGIC;
		gpio  : in STD_LOGIC;
		ppsout : out STD_LOGIC;
		gpio_en: out STD_LOGIC;
		
		led	: out STD_LOGIC_VECTOR(7 downto 0);
		cr1	: out STD_LOGIC;
		cr2	: out STD_LOGIC;
		clk100 : in STD_LOGIC
	);
end TTSCounterPPSCR;

architecture arch of TTSCounterPPSCR is

	component PPSToCR is 
		port ( 
			trigger : in  STD_LOGIC;
			clk	  : in  STD_LOGIC;
			serial_out : out  STD_LOGIC
			);
	end component;
	
	signal ti_clk     : STD_LOGIC;
	signal ok1        : STD_LOGIC_VECTOR(30 downto 0);
	signal ok2        : STD_LOGIC_VECTOR(16 downto 0);
-- note that the multiplier of '17' is the number of WireOuts+TriggerOuts
	signal ok2s       : STD_LOGIC_VECTOR(17*14-1 downto 0);

-- WireIns
	signal ep00wire   : STD_LOGIC_VECTOR(15 downto 0); -- system control
	
-- WireOuts
	signal ep20wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 1 reading MSB
	signal ep21wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 1 reading LSB
	
	signal ep22wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 2 reading
	signal ep23wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 2 reading
	
	signal ep24wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 3 reading
	signal ep25wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 3 reading
	
	signal ep26wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 4 reading
	signal ep27wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 4 reading
	
	signal ep28wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 5 reading
	signal ep29wire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 5 reading
	
	signal ep2awire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 6 reading
	signal ep2bwire   : STD_LOGIC_VECTOR(15 downto 0); -- counter 6 reading

	signal ep2cwire   : STD_LOGIC_VECTOR(15 downto 0); -- system status
	
-- TriggerOuts
	signal ep60trig   : STD_LOGIC_VECTOR(15 downto 0); -- counter trigger status
	
	signal extclk		: STD_LOGIC;
	signal sysCtrl    : STD_LOGIC_VECTOR(15 downto 0);  
	signal sysStat    : STD_LOGIC_VECTOR(15 downto 0); 
	
	signal trigA : STD_LOGIC;
	
-- per TICounter signals
--	TIC1
	signal trigB1 :STD_LOGIC;
	signal tint1  :STD_LOGIC_VECTOR(31 downto 0);
	signal dataReady1: STD_LOGIC;
	signal ledPulse1: STD_LOGIC;

--	TIC2
	signal trigB2 :STD_LOGIC;
	signal tint2  :STD_LOGIC_VECTOR(31 downto 0);
	signal dataReady2: STD_LOGIC;
	signal ledPulse2: STD_LOGIC;
	
--	TIC3
	signal trigB3 :STD_LOGIC;
	signal tint3  :STD_LOGIC_VECTOR(31 downto 0);
	signal dataReady3: STD_LOGIC;
	signal ledPulse3: STD_LOGIC;
	
--	TIC4
	signal trigB4 :STD_LOGIC;
	signal tint4  :STD_LOGIC_VECTOR(31 downto 0);
	signal dataReady4: STD_LOGIC;
	signal ledPulse4: STD_LOGIC;
	
--	TIC5
	signal trigB5 :STD_LOGIC;
	signal tint5  :STD_LOGIC_VECTOR(31 downto 0);
	signal dataReady5: STD_LOGIC;
	signal ledPulse5: STD_LOGIC;

--	TIC6
	signal trigB6 :STD_LOGIC;
	signal tint6  :STD_LOGIC_VECTOR(31 downto 0);
	signal dataReady6: STD_LOGIC;
	signal ledPulse6: STD_LOGIC;
	signal ct6b: STD_LOGIC; -- this is internally connected
	
-- DCM signals
	signal clko0,clk180,clk270,clk2X,clk2x180,clk90,clkdv,clkfx,clkfx180,locked,psdone : STD_LOGIC;
	signal status: STD_LOGIC_VECTOR(7 downto 0);
	signal clkin,dssen,psclk,psen,psincdec,rst:STD_LOGIC; 
	
begin

hi_muxsel  <= '0'; -- required 

-- DCM_SP: Digital Clock Manager
--         Spartan-6
-- Xilinx HDL Language Template, version 13.3

   DCM_SP_200 : DCM_SP
   generic map (
      CLKDV_DIVIDE => 2.0,                   -- CLKDV divide value
                                             -- (1.5,2,2.5,3,3.5,4,4.5,5,5.5,6,6.5,7,7.5,8,9,10,11,12,13,14,15,16).
      CLKFX_DIVIDE => 1,                     -- Divide value on CLKFX outputs - D - (1-32)
      CLKFX_MULTIPLY => 2,                   -- Multiply value on CLKFX outputs - M - (2-32)
      CLKIN_DIVIDE_BY_2 => FALSE,            -- CLKIN divide by two (TRUE/FALSE)
      CLKIN_PERIOD => 10.0,                  -- Input clock period specified in nS
      CLKOUT_PHASE_SHIFT => "NONE",          -- Output phase shift (NONE, FIXED, VARIABLE)
      CLK_FEEDBACK => "1X",                  -- Feedback source (NONE, 1X, 2X)
      DESKEW_ADJUST => "SYSTEM_SYNCHRONOUS", -- SYSTEM_SYNCHRNOUS or SOURCE_SYNCHRONOUS
      DFS_FREQUENCY_MODE => "LOW",           -- Unsupported - Do not change value
      DLL_FREQUENCY_MODE => "LOW",           -- Unsupported - Do not change value
      DSS_MODE => "NONE",                    -- Unsupported - Do not change value
      DUTY_CYCLE_CORRECTION => TRUE,         -- Unsupported - Do not change value
      FACTORY_JF => X"c080",                 -- Unsupported - Do not change value
      PHASE_SHIFT => 0,                      -- Amount of fixed phase shift (-255 to 255)
      STARTUP_WAIT => FALSE                  -- Delay config DONE until DCM_SP LOCKED (TRUE/FALSE)
   )
   port map (
      CLK0 => clko0,        -- 1-bit output: 0 degree clock output
      CLK180 => clk180,     -- 1-bit output: 180 degree clock output
      CLK270 => clk270,     -- 1-bit output: 270 degree clock output
      CLK2X => clk2X,       -- 1-bit output: 2X clock frequency clock output
      CLK2X180 => clk2X180, -- 1-bit output: 2X clock frequency, 180 degree clock output
      CLK90 => clk90,       -- 1-bit output: 90 degree clock output
      CLKDV => clkdv,       -- 1-bit output: Divided clock output
      CLKFX => clkfx,       -- 1-bit output: Digital Frequency Synthesizer output (DFS)
      CLKFX180 => clkfx180, -- 1-bit output: 180 degree CLKFX output
      LOCKED => locked,     -- 1-bit output: DCM_SP Lock Output
      PSDONE => psdone,     -- 1-bit output: Phase shift done output
      STATUS => status,     -- 8-bit output: DCM_SP status output
      CLKFB => clko0,       -- 1-bit input: Clock feedback input
      CLKIN => clk100,      -- 1-bit input: Clock input
      DSSEN => '0',         -- 1-bit input: Unsupported, specify to GND.
      PSCLK => psclk,       -- 1-bit input: Phase shift clock input
      PSEN => psen,         -- 1-bit input: Phase shift enable
      PSINCDEC => psincdec, -- 1-bit input: Phase shift increment/decrement input
      RST => rst            -- 1-bit input: Active high reset input
   );

extclk <= clkfx;

-- Common start trigger for all counters
chanATrig : OneShot port map (trigger=>refpps,clk=>extclk,pulse=>trigA);

-- TIC1
chan1BTrig: OneShot port map (trigger=>ct1b,clk=>extclk,pulse=>trigB1);

tic1	: TICounter32 port map (startTrig => trigA,stopTrig =>trigB1,clk=>extclk,
			tint => tint1, dataReady => dataReady1, ledPulse => ledPulse1);
-- TIC2
chan2BTrig: OneShot port map (trigger=>ct2b,clk=>extclk,pulse=>trigB2);

tic2	: TICounter32 port map (startTrig => trigA,stopTrig =>trigB2,clk=>extclk,
			tint => tint2, dataReady => dataReady2, ledPulse => ledPulse2);

-- TIC3
chan3BTrig: OneShot port map (trigger=>ct3b,clk=>extclk,pulse=>trigB3);

tic3	: TICounter32 port map (startTrig => trigA,stopTrig =>trigB3,clk=>extclk,
			tint => tint3, dataReady => dataReady3, ledPulse => ledPulse3);

-- TIC4
chan4BTrig: OneShot port map (trigger=>ct4b,clk=>extclk,pulse=>trigB4);

tic4	: TICounter32 port map (startTrig => trigA,stopTrig =>trigB4,clk=>extclk,
			tint => tint4, dataReady => dataReady4, ledPulse => ledPulse4);

-- TIC5
chan5BTrig: OneShot port map (trigger=>ct5b,clk=>extclk,pulse=>trigB5);

tic5	: TICounter32 port map (startTrig => trigA,stopTrig =>trigB5,clk=>extclk,
			tint => tint5, dataReady => dataReady5, ledPulse => ledPulse5);

-- TIC6
chan6BTrig: OneShot port map (trigger=>ct6b,clk=>extclk,pulse=>trigB6);

tic6	: TICounter32 port map (startTrig => trigA,stopTrig =>trigB6,clk=>extclk,
			tint => tint6, dataReady => dataReady6, ledPulse => ledPulse6);
			

-- 1 pps to ASCII carriage-return

ppscr1: PPSToCR port map (trigger=>refpps,clk=>clk100,serial_out=>cr1);
ppscr2: PPSToCR port map (trigger=>ct2b,clk=>clk100,serial_out=>cr2);
		
-- outputs to the host

ep21wire <= tint1(31 downto 16);
ep20wire <= tint1(15 downto 0);

ep23wire <= tint2(31 downto 16);
ep22wire <= tint2(15 downto 0);

ep25wire <= tint3(31 downto 16);
ep24wire <= tint3(15 downto 0);

ep27wire <= tint4(31 downto 16);
ep26wire <= tint4(15 downto 0);

ep29wire <= tint5(31 downto 16);
ep28wire <= tint5(15 downto 0);

ep2bwire <= tint6(31 downto 16);
ep2awire <= tint6(15 downto 0);

-- counter trigger state
ep60trig <= "0000000000" & dataReady6 & dataReady5 & dataReady4 & dataReady3 & dataReady2 & dataReady1; 

-- system config/status
-- bit 2->0: pps out source
-- bit 3   : GPIO enabled
-- bit 4   : DCM locked
ep2cwire <= sysStat;

sysStat <= "00000000000" & locked & sysCtrl(3) & sysCtrl(2 downto 0);

-- system control register
-- bits 2->0 : selection of output 1 pps source  
-- bit  3    : enable external I/O on GPIO pin
sysCtrl <= ep00wire;  -- endpoints are registers, it seems

-- MUX to select pps out source
with (sysCtrl(2 downto 0)) select
	ppsout <= ct1b when "001",
				 ct2b when "010",  -- this is the default
				 ct3b when "011",
				 ct4b when "100",
				 ct5b when "101",
		       ct6b when "110",
				 refpps when others;

gpio_en <= sysCtrl(3);
-- gpio_en <= '1';
ct6b <= gpio; -- default setup is as input to the 6th counter. In other applications, it might be an input to something else

-- outputs to FPGA 'peripherals'
-- bits 0-5: counter 1 pps in
-- bit    6: ext_io_en (for debugging)
-- bit    7: DCM lock

led <= (not locked) & (not sysCtrl(3)) & (not ledPulse6) & (not ledPulse5) & (not ledPulse4) & (not ledPulse3) & (not ledPulse2) & (not ledPulse1);

-- Instantiate the okHost and connect endpoints
okHI : okHost port map (
		hi_in=>hi_in, hi_out=>hi_out, hi_inout=>hi_inout,
		ti_clk=>ti_clk, ok1=>ok1, ok2=>ok2);

okWO : okWireOR     generic map (N=>14) port map (ok2=>ok2, ok2s=>ok2s);

-- WireOuts are updated synchronously with the host interface clock.
-- In particular, the counter readings have to cross a clock domain. This can be done without a synchronizer because
-- the counters are always read after a value has been stored in the counter's output register.
-- So, no synchronizers to cross the clock domains.
ep20 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 1*17-1 downto 0*17 ), ep_addr=>x"20", ep_datain=>ep20wire);
ep21 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 2*17-1 downto 1*17 ), ep_addr=>x"21", ep_datain=>ep21wire);

ep22 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 3*17-1 downto 2*17 ), ep_addr=>x"22", ep_datain=>ep22wire);
ep23 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 4*17-1 downto 3*17 ), ep_addr=>x"23", ep_datain=>ep23wire);

ep24 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 5*17-1 downto 4*17 ), ep_addr=>x"24", ep_datain=>ep24wire);
ep25 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 6*17-1 downto 5*17 ), ep_addr=>x"25", ep_datain=>ep25wire);

ep26 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 7*17-1 downto 6*17 ), ep_addr=>x"26", ep_datain=>ep26wire);
ep27 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 8*17-1 downto 7*17 ), ep_addr=>x"27", ep_datain=>ep27wire);

ep28 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 9*17-1 downto 8*17 ), ep_addr=>x"28", ep_datain=>ep28wire);
ep29 : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 10*17-1 downto 9*17 ), ep_addr=>x"29", ep_datain=>ep29wire);

ep2a : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 11*17-1 downto 10*17 ), ep_addr=>x"2a", ep_datain=>ep2awire);
ep2b : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 12*17-1 downto 11*17 ), ep_addr=>x"2b", ep_datain=>ep2bwire);

-- system status
ep2c : okWireOut    port map (ok1=>ok1, ok2=>ok2s( 13*17-1 downto 12*17 ), ep_addr=>x"2c", ep_datain=>ep2cwire);

-- system control register 
-- WireIns map between 0x00 and 0x1f
ep00 : okWireIn    port map (ok1=>ok1, ep_addr=>x"00", ep_dataout=>ep00wire);

-- counter trigger state
ep60 : okTriggerOut port map (ok1=>ok1,ok2=>ok2s( 14*17-1 downto 13*17 ),  ep_addr=>x"60", ep_clk=>extclk, ep_trigger=>ep60trig);

end arch;