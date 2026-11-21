
int a, b, c, x;
int i, s;

begin
    // ------------------------------------------------
    // TEST -O1: Constant Folding / Branch Optimization
    // ------------------------------------------------

    if (1 > 2)
    begin
        c = 999;
    end
    else
    begin
        c = 7;
    end

    print c;


    // ------------------------------------------------
    // TEST -O2: Global Constant Propagation
    // ------------------------------------------------

    print "Please enter b: ";
    read b;

    a = 10;

    if (b > 5)
    begin
        x = a;
    end
    else
    begin
        x = a;
    end

    print x;


    // ------------------------------------------------
    // TEST -O3: Loop Optimization
    // ------------------------------------------------

    i = 0;
    s = 0;

    while (i < 4)
    begin
        s = s + i;
        i = i + 1;
    end

    print s;
end
