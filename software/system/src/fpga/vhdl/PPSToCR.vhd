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
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.TRIGGERS.OneShot;

entity PPSToCR is
	port(	trigger : in  STD_LOGIC;
			clk	  : in  STD_LOGIC;
			serial_out : out  STD_LOGIC);
end PPSToCR;

architecture Behavioral of PPSToCR is

	type fsm_type is (ST_WAIT,ST_TRIGGERED);
	signal curr_state: fsm_type;
	signal trig_pulse: STD_LOGIC;

	constant ser_cr : STD_LOGIC_VECTOR(0 to 9) := "0101100001";
		
begin

process (clk)
	variable clk_div_cnt: natural range 0 to 1023:=0; 
	variable bit_cnt: natural range 0 to 15:=0; 
begin
	if rising_edge(clk) then
		case curr_state is
			when ST_WAIT =>
				curr_state <= ST_WAIT;
				serial_out <= '1';
				if trig_pulse='1' then
					curr_state <= ST_TRIGGERED;
				end if;
			when ST_TRIGGERED=>
				serial_out<=ser_cr(bit_cnt);
				clk_div_cnt:=clk_div_cnt+1;
				if clk_div_cnt = 868 then -- main clock divider 
					bit_cnt := bit_cnt+1;
					clk_div_cnt:=0;
					if bit_cnt=10 then
						curr_state <= ST_WAIT;
						bit_cnt:=0;
						clk_div_cnt:=0;
					end if;
				end if;
			when others =>
				curr_state <= ST_WAIT;
				serial_out <= '1';
		end case;
				
	end if;
end process;

trigin: OneShot port map (trigger=>trigger,clk=>clk,pulse=>trig_pulse);

end Behavioral;
