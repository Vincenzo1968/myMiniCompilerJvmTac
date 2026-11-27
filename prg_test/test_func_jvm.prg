void setb(ref bool b, bool v);
void apx(ref string s, string t);
void incr(ref real x, real d);
void fwd(ref int a);
void viaFwd(ref int a);
real pot(real b, int e);
int cercaPrimo(int da, int a);
void toccaGlob(int i, int j, int v);
int sommaMat();
real media(real a, int n, real b, bool dbl);
void stampaSe(int x);
string dup(string s);
int locArr(int n);

bool gb;
real gr;
string gs;
array mat[3][4] of int;
int gi;

begin
	bool b;
	string s;
	real x;
	int k;
	int q;
	b = false;
	setb(b, true);
	if (b) print "b vero\n";
	s = "uno";
	apx(s, "-due");
	apx(s, "-tre");
	print s; print "\n";
	x = 1.5;
	incr(x, 2.25);
	incr(x, 0.25);
	print x; print "\n";
	k = 10;
	viaFwd(k);
	print k; print "\n";
	print pot(2.0, 10); print "\n";
	print pot(1.5, 3); print "\n";
	print cercaPrimo(90, 120); print "\n";
	toccaGlob(1, 2, 7);
	toccaGlob(2, 3, 5);
	print sommaMat(); print "\n";
	print mat[1][2]; print "\n";
	print media(3.0, 2, 5.0, true); print "\n";
	print media(3.0, 2, 5.0, false); print "\n";
	stampaSe(0);
	stampaSe(4);
	gs = "g";
	gs = dup(gs) + dup("z");
	print gs; print "\n";
	gr = 0.5;
	gb = true;
	gi = 3;
	print locArr(5); print "\n";
	print gr; print "\n";
	q = 0;
	while (q < 3)
	begin
		fwd(gi);
		q = q + 1;
	end
	print gi; print "\n";
	if (gb) print "gb vero\n";
end

void setb(ref bool b, bool v)
begin
	b = v;
end

void apx(ref string s, string t)
begin
	s = s + t;
end

void incr(ref real x, real d)
begin
	x = x + d;
end

void fwd(ref int a)
begin
	a = a + 2;
end

void viaFwd(ref int a)
begin
	fwd(a);
	fwd(a);
end

real pot(real b, int e)
begin
	real r;
	int i;
	r = 1.0;
	i = 0;
	while (i < e)
	begin
		r = r * b;
		i = i + 1;
	end
	return r;
end

int cercaPrimo(int da, int a)
begin
	int n;
	int d;
	bool primo;
	n = da;
	while (n <= a)
	begin
		primo = true;
		d = 2;
		while (d * d <= n)
		begin
			if (n - (n / d) * d == 0) primo = false;
			d = d + 1;
		end
		if (primo) return n;
		n = n + 1;
	end
	return 0 - 1;
end

void toccaGlob(int i, int j, int v)
begin
	mat[i][j] = v;
	gr = gr + 1.0;
end

int sommaMat()
begin
	int i;
	int j;
	int s;
	s = 0;
	i = 0;
	while (i < 3)
	begin
		j = 0;
		while (j < 4)
		begin
			s = s + mat[i][j];
			j = j + 1;
		end
		i = i + 1;
	end
	return s;
end

real media(real a, int n, real b, bool dbl)
begin
	real r;
	r = (a * n + b) / (n + 1);
	if (dbl) r = r * 2.0;
	return r;
end

void stampaSe(int x)
begin
	if (x == 0) return;
	print "x="; print x; print "\n";
end

string dup(string s)
begin
	return s + s;
end

int locArr(int n)
begin
	array t[6] of int;
	int i;
	int s;
	real loc;
	loc = 2.5;
	i = 0;
	while (i < n)
	begin
		t[i] = i * i;
		i = i + 1;
	end
	s = 0;
	i = 0;
	while (i < n)
	begin
		s = s + t[i];
		i = i + 1;
	end
	gr = gr + loc;
	return s;
end
