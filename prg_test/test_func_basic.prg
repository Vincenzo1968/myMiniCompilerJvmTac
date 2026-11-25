int fact(int n);
void incr(ref int x, int d);
real media(array v[] of real, int n);
string saluta(string nome);
bool pari(int n);
void riempi(array a[] of int, int n);
int somma(array a[] of int, int n);
int fib(int n);
void scambia(ref int a, ref int b);

int g;
array v[3] of real;
array a[4] of int;

begin
	int i;
	int k;
	int x;
	int y;
	string s;
	g = 100;
	print fact(5); print "\n";
	k = 10;
	incr(k, 5);
	print k; print "\n";
	v[0] = 1.5; v[1] = 2.5; v[2] = 5.0;
	print media(v, 3); print "\n";
	s = saluta("Mondo");
	print s; print "\n";
	print saluta("a") + saluta("b") + "\n";
	if (pari(4)) print "4 pari\n";
	if (!pari(7)) print "7 dispari\n";
	riempi(a, 4);
	print somma(a, 4); print "\n";
	print a[3]; print "\n";
	print fib(10); print "\n";
	x = 1; y = 2;
	scambia(x, y);
	print x; print " "; print y; print "\n";
	i = 0;
	while (i < 3)
	begin
		print fact(i + 1); print " ";
		i = i + 1;
	end
	print "\n";
	print g; print "\n";
end

int fact(int n)
begin
	if (n <= 1)
		return 1;
	return n * fact(n - 1);
end

void incr(ref int x, int d)
begin
	x = x + d;
	g = g + 1;
end

real media(array v[] of real, int n)
begin
	int i;
	real t;
	t = 0.0;
	i = 0;
	while (i < n)
	begin
		t = t + v[i];
		i = i + 1;
	end
	return t / n;
end

string saluta(string nome)
begin
	return "Ciao " + nome;
end

bool pari(int n)
begin
	return n - (n / 2) * 2 == 0;
end

void riempi(array a[] of int, int n)
begin
	int i;
	i = 0;
	while (i < n)
	begin
		a[i] = i * i;
		i = i + 1;
	end
end

int somma(array a[] of int, int n)
begin
	int i;
	int t;
	t = 0;
	i = 0;
	while (i < n)
	begin
		t = t + a[i];
		i = i + 1;
	end
	return t;
end

int fib(int n)
begin
	if (n < 2)
		return n;
	return fib(n - 1) + fib(n - 2);
end

void scambia(ref int a, ref int b)
begin
	int t;
	t = a;
	a = b;
	b = t;
end
