
real r;
array mat[2][2] of int;
int i;

begin
    r = 5 / 2;
    mat[0][0] = 10;
    mat[1][1] = 20;

    i = 0;
    do
    begin
        print mat[i][i];
        i = i + 1;
    end
    while (i < 2)

    print r;
end
