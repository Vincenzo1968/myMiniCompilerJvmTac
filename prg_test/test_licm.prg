
int i, n, a, b, s;

begin
	print "Inserisci n: ";
    read n;
    
    print "Inserisci a: ";
    read a;
    
    print "Inserisci b: ";
    read b;

    i = 0;
    s = 0;
    while (i < n)
    begin
        s = s + (a * b + 1) + (i * 2);
        i = i + 1;
    end

    print "Eqque qua: ";
    print s;
    print "\n";
end
