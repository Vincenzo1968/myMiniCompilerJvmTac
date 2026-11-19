int a, b, c, x, y;

begin
    print "=== Test 1: Missing semicolon ===";
    a = 10
    b = 20;  // Il parser dovrebbe recuperare dal ';' mancante sopra e parsare questa riga correttamente

    print "=== Test 2: Undeclared variable ===";
    c = a + b + variabile_sconosciuta; // Errore semantico: identificatore non dichiarato

    print "=== Test 3: Type mismatch ===";
    x = 5;
    y = "ciao"; // Errore semantico: assegnamento di stringa a variabile int

    print "=== Test 4: Malformed if statement ===";
    if (a > b  // Errore sintattico: manca la parentesi tonda chiusa ')'
        x = 99; 
    end

    print "=== Test 5: Valid statement after errors ===";
    // Se il recovery funziona, questa istruzione verrà parsata ed eseguita/compilata correttamente
    c = a * b;
    print "Valore finale di c: ";
    print c;
end
