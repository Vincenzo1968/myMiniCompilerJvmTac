int a, b, x, y;

begin
    read a;
    read b;

    x = 10;

    if (1 > 2)
    begin
        x = 999;
    end
    else
    begin
        x = a * b + 1;
    end

    y = b * a + 2;

    print x;
    print y;
end
