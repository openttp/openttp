----------------------------------------------------------------------------------
--	TICounters - a collection of counters
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
--  Modification history 
--	 2013-10-21 MJW First version.
--  

library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.std_logic_arith.all;
use IEEE.std_logic_misc.all;
use IEEE.std_logic_unsigned.all;
use work.TRIGGERS.OneShot;

-- 
-- 32 bit time-interval measurement
--

entity TICounter32 is
	port(	startTrig	:	in	STD_LOGIC;
			stopTrig	:	in STD_LOGIC;
			clk	:	in  STD_LOGIC;		
			tint	:	out  STD_LOGIC_VECTOR(31 downto 0); -- the time interval measurement
			dataReady	:	out STD_LOGIC;
			ledPulse	: out STD_LOGIC
			);
end TICounter32;

architecture RTL of TICounter32 is

	signal cnt	:STD_LOGIC_VECTOR(29 downto 0);
	signal savereg  :STD_LOGIC_VECTOR(31 downto 0);
	signal triggered	:STD_LOGIC;
	signal delay1: STD_LOGIC;
	
begin

	-- Free running counter with synchronous reset
	-- The start trigger must only be one period of 'clk' long so it will 
	-- be typically be driven via a one-shot
	process (clk)
	begin
		if (rising_edge(clk)) then
			if (startTrig='1' ) then 
				cnt<= (others => '0');
			else
				cnt<= cnt + 1;
			end if;
		end if;
	end process;
	
	-- Register for saving the counter reading
	-- Similar to the start trigger, the stop trigger must be only one period of 'clk' long
	process (clk) 
	begin
		if rising_edge(clk) then
			if delay1 ='1' then
				savereg <= cnt & "00"; -- bottom two bits will eventually set by a delay chain measurement
			end if;
		end if;
	end process;

	-- Delay of one clock period to synchronise clocking of output register
	-- with data availablity
	
	process (clk)
	begin
		if rising_edge(clk) then
			delay1 <= stopTrig;
		end if;
	end process;

	-- 'Data ready' signal - just a delayed 'B' trigger
	process (clk)
	begin
		if rising_edge(clk) then
			triggered <= delay1;
		end if;
	end process;

	tint <= savereg;
	
	-- Kind of a hack
	-- A long pulse, suitable for driving an LED or such like
	-- Since it's only a visual indication we don't care that there's a delay between the trigger
	-- pulse and the output pulse
	-- At 100 MHz, the output pulse is 20 ms long (and delayed by up to 20 ms) if bit 21 is used
	
	ledDriver: OneShot port map (trigger=>triggered,clk=>cnt(21),pulse=>ledPulse);
	
	dataReady <= triggered;
	
end RTL;

-- 
--
--

library IEEE;
use IEEE.STD_LOGIC_1164.all;

package TICounters is
	component TICounter32 is port ( 
		startTrig	:	in	STD_LOGIC;
		stopTrig	:	in STD_LOGIC;
		clk	:	in  STD_LOGIC;
		tint	:	out  STD_LOGIC_VECTOR(31 downto 0);
		dataReady	:	out STD_LOGIC;
		ledPulse	: out STD_LOGIC
		);
	end component;
end TICounters;


