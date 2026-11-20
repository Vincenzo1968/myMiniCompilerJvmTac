int a, b, c, x, y;
int arr[5];
real r;
bool flag;
string s;

// Test error recovery nelle dichiarazioni (var_list)
int d = "errore_tipo", e = 42;
int f = a + 1, g = 10;

begin
    print "=== Test 1: Missing semicolon ===";
    a = 10
    b = 20;

    print "=== Test 2: Undeclared variable ===";
    c = a + b + variabile_sconosciuta;

    print "=== Test 3: Type mismatch ===";
    x = 5;
    y = "ciao";
    r = true;
    flag = 123;
    s = 99;

    print "=== Test 4: Array errors (index type, dimension count, non-array) ===";
    arr["indice_non_intero"] = 1;
    arr[0][1] = 2;
    x[0] = 3;
    c = arr[1][2] + arr["bad"];

    print "=== Test 5: Invalid read target ===";
    read 10;
    read (a + 1);

    print "=== Test 6: Malformed if and do-while ===";
    if (a > b
        x = 99;

    do
        x = x + 1;
    while ("stringa_non_booleana")

    print "=== Test 7: Uninitialized variable in expr ===";
    c = f + 1;

    print "=== Test 8: Valid statement after errors ===";
    c = a * b;
    print "Valore finale di c: ";
    print c;
end
