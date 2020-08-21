#!/usr/bin/perl -w

print "Exporting pins and setting as outputs\n";

for ($c=0;$c<=8;$c++){
	for ($p=0;$p<=7;$p++){
		$pin = $p;
		if ($c > 0){
			$pin = $c . $p;
		}
		if (!(-e "/sys/class/gpio/gpio$pin")){
			`echo $pin > /sys/class/gpio/export`;
		}
		`echo out > /sys/class/gpio/gpio$pin/direction`;
	}
}

print "Setting high\n";

for ($c=0;$c<=8;$c++){
	for ($p=0;$p<=7;$p++){
		$pin = $p;
		if ($c > 0){
			$pin = $c . $p;
		}
		`echo 1 > /sys/class/gpio/gpio$pin/value`;
	}
}


print "Setting low\n";

for ($c=0;$c<=8;$c++){
	print "Port $c OFF\n";
	for ($p=0;$p<=7;$p++){
		$pin = $p;
		if ($c > 0){
			$pin = $c . $p;
		}
		`echo 0 > /sys/class/gpio/gpio$pin/value`;
	}
	sleep(1);
}

