#!/usr/bin/expect
for {set i 1} {$i < 100} {incr i} {
	spawn telnet localhost 10000
	expect "id"
	send "902001\n"
	expect "respect"
	send "1 2 3\n"
	expect "new"
	wait 
	spawn telnet localhost 10000
	expect "id"
	send "1000000\n"
	expect "Err"
	wait 
	spawn telnet localhost 10000
	expect "id"
	send "902001\n"
	expect "respect"
	send "1 1 1\n"
	expect "Error"
	wait 
}
