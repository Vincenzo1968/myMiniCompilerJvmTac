
int i, n, a, b, s;

begin
    read n;
    read a;
    read b;

    i = 0;
    s = 0;
    while (i < n)
    begin
        s = s + (a * b + 1) + (i * 2);
        i = i + 1;
    end

    print s;
end
