array a[5] of int;
array r[3] of real;
array b[4] of bool;
array s[3] of string;
array m[3][4] of int;

begin
	int i;
	int j;
	int t;
	i = 0;
	while (i < 5)
	begin
		a[i] = i * i + 1;
		i = i + 1;
	end
	t = 0;
	i = 0;
	while (i < 5)
	begin
		t = t + a[i];
		i = i + 1;
	end
	print t; print "\n";

	r[0] = 1.5; r[1] = r[0] * 2.0; r[2] = r[1] + a[2];
	print r[2]; print "\n";

	b[1] = true; b[2] = a[3] > 5;
	print b[0]; print b[1]; print b[2]; print "\n";

	s[0] = "ciao"; s[2] = s[0] + " mondo";
	print s[2]; print "\n";
	print "[" + s[1] + "]\n";

	i = 0;
	while (i < 3)
	begin
		j = 0;
		while (j < 4)
		begin
			m[i][j] = i * 10 + j;
			j = j + 1;
		end
		i = i + 1;
	end
	print m[2][3]; print " "; print m[1][2]; print " "; print m[0][0]; print "\n";
	a[a[1]] = 99;
	print a[2]; print "\n";
end
