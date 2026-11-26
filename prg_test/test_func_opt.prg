int g;
int cnt;
int bump(int k);
int sq(int x);
void setg(int v);
void fill(array a[] of int, int n);
int sum(array a[] of int, int n);
void swap(ref int a, ref int b);

begin
	int i;
	int z;
	int t;
	array v[6] of int;
	g = 1;
	cnt = 0;
	setg(7);
	print g; print "\n";
	z = bump(2) * 0;
	print cnt; print "\n";
	t = 0;
	i = 0;
	while (i < 4)
	begin
		t = t + sq(i) + bump(1);
		i = i + 1;
	end
	print t; print " "; print cnt; print "\n";
	fill(v, 6);
	print sum(v, 6); print "\n";
	i = 3; t = 9;
	swap(i, t);
	print i; print " "; print t; print "\n";
	g = 5;
	t = sq(g) + sq(g);
	print t; print "\n";
end

int bump(int k)
begin
	cnt = cnt + k;
	return cnt;
end

int sq(int x)
begin
	int r;
	r = x * x;
	return r;
end

void setg(int v)
begin
	g = v;
	g = g + 0;
end

void fill(array a[] of int, int n)
begin
	int i;
	i = 0;
	while (i < n)
	begin
		a[i] = i * 3;
		i = i + 1;
	end
end

int sum(array a[] of int, int n)
begin
	int i;
	int s;
	s = 0;
	i = 0;
	while (i < n)
	begin
		s = s + a[i] * 2;
		i = i + 1;
	end
	return s;
end

void swap(ref int a, ref int b)
begin
	int t;
	t = a;
	a = b;
	b = t;
end
