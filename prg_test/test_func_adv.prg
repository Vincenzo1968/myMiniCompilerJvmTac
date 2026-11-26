real mezzo(int n);
int somma7(int a, int b, int c, int d, int e, int f, int g);
real mix(real a, int b, real c);
bool pari(int n);
bool dispari(int n);
void conta(ref int tot, int n);
void riempi(array v[] of int, int n);
void raddoppia(array v[] of int, int n);
int totale(array v[] of int, int n);
real somma_r(array r[] of real, int n);
string ripeti(string s, int n);
int max2(int a, int b);
void azzera(ref real x);
void nulla();
int fattoriale(int n);
void acc(ref int s, int n);

int cont;
array glob[4] of int;
real ggg;
string nome;

begin
	int i;
	int t;
	array loc[5] of int;
	array rr[3] of real;
	real z;
	string s;

	cont = 0;
	print mezzo(7); print "\n";
	print somma7(1, 2, 3, 4, 5, 6, 7); print "\n";
	print somma7(max2(2, 1) + 1, max2(3, 9), fattoriale(3), 1, 1, 1, 1); print "\n";
	print mix(1, 2, 3.5); print "\n";
	if (pari(10) & dispari(7)) print "mutua ok\n";
	if (pari(7)) print "ERRORE\n";

	t = 0;
	conta(t, 5);
	conta(t, 3);
	print t; print "\n";

	riempi(loc, 5);
	raddoppia(loc, 5);
	print totale(loc, 5); print "\n";
	riempi(glob, 4);
	print totale(glob, 4); print "\n";
	print glob[3]; print "\n";

	rr[0] = 1.5; rr[1] = 2.5; rr[2] = 3.0;
	print somma_r(rr, 3); print "\n";

	s = ripeti("ab", 3);
	print s; print "\n";
	nome = ripeti("x", 2) + "-" + ripeti("y", 2);
	print nome; print "\n";

	z = 9.5;
	azzera(z);
	print z; print "\n";
	nulla();
	nulla();
	print cont; print "\n";

	t = 0;
	i = 1;
	while (max2(i, 3) < 5)
	begin
		acc(t, i);
		i = i + 1;
	end
	print t; print "\n";
	print fattoriale(7); print "\n";
	ggg = mix(2, 3, 4.5) * 2;
	print ggg; print "\n";
end

real mezzo(int n)
begin
	return n / 2.0;
end

int somma7(int a, int b, int c, int d, int e, int f, int g)
begin
	return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7;
end

real mix(real a, int b, real c)
begin
	return a * b + c;
end

bool pari(int n)
begin
	if (n == 0) return true;
	return dispari(n - 1);
end

bool dispari(int n)
begin
	if (n == 0) return false;
	return pari(n - 1);
end

void conta(ref int tot, int n)
begin
	int i;
	i = 0;
	while (i < n)
	begin
		tot = tot + 1;
		i = i + 1;
	end
end

void riempi(array v[] of int, int n)
begin
	int i;
	i = 0;
	while (i < n)
	begin
		v[i] = i + 1;
		i = i + 1;
	end
end

void raddoppia(array v[] of int, int n)
begin
	int i;
	i = 0;
	while (i < n)
	begin
		v[i] = v[i] * 2;
		i = i + 1;
	end
end

int totale(array v[] of int, int n)
begin
	int i;
	int s;
	s = 0;
	i = 0;
	while (i < n)
	begin
		s = s + v[i];
		i = i + 1;
	end
	return s;
end

real somma_r(array r[] of real, int n)
begin
	int i;
	real s;
	s = 0.0;
	i = 0;
	while (i < n)
	begin
		s = s + r[i];
		i = i + 1;
	end
	return s;
end

string ripeti(string s, int n)
begin
	string r;
	int i;
	r = "";
	i = 0;
	while (i < n)
	begin
		r = r + s;
		i = i + 1;
	end
	return r;
end

int max2(int a, int b)
begin
	if (a > b) return a;
	return b;
end

void azzera(ref real x)
begin
	x = 0.0;
end

void nulla()
begin
	cont = cont + 1;
end

int fattoriale(int n)
begin
	if (n <= 1) return 1;
	return n * fattoriale(n - 1);
end

void acc(ref int s, int n)
begin
	s = s + n * n;
	if (n > 100) return;
end
