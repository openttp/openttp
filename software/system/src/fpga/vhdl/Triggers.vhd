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
--

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity OneShot is
    Port ( trigger : in  STD_LOGIC;
           clk : in  STD_LOGIC;
           pulse : out  STD_LOGIC);
end OneShot;

architecture Behavioral of OneShot is

signal QA: std_logic := '0';
signal QB: std_logic := '0';

begin

pulse <= QB;

process (trigger, QB)
begin
	if QB='1' then
		QA <= '0';
	elsif (rising_edge(trigger)) then
		QA <= '1';
end if;
end process;

process (clk)
begin
	if rising_edge(clk) then
		QB <= QA;
	end if;
end process;

end Behavioral;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
package TRIGGERS is
	component OneShot is port ( 
		trigger : in  STD_LOGIC;
		clk : in  STD_LOGIC;
		pulse : out  STD_LOGIC);
	end component;
end TRIGGERS;
