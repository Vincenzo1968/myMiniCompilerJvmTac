int a = 1;
real r = 1.5;
string s = "g";

begin
	print a; print " "; print r; print " "; print s; print "\n";

	begin
		string a = "inner-a";
		bool r = true;
		int s = 42;
		print a; print " ";
		if (r) print "T"; else print "F";
		print " "; print s; print "\n";

		begin
			real a = 2.5;
			a = a * 2;
			print a; print "\n";
		end

		print a; print "\n";
	end

	begin
		real a = 9.75;
		int q = 3;
		print a; print " "; print q; print "\n";
	end

	a = a + 10;
	print a; print "\n";
	print r; print " "; print s; print "\n";
end
