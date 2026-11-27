struct V { int n; array d[3] of int; };
struct Pt { int x; int y; };
struct Box { Pt lo; Pt hi; array pts[2] of Pt; };

V gv;
Pt gp;

void riempi(ref V v, int k);
int tot(ref V v);
int area(ref Box b);
Pt copia(Pt a);
void azzera(ref Pt p);

begin
	V v;
	Box b;
	Pt p, q;
	array bs[2] of Box;
	int i;
	riempi(v, 4);
	print tot(v); print "\n";
	riempi(gv, 2);
	print tot(gv); print "\n";
	b.lo.x = 1; b.lo.y = 2; b.hi.x = 11; b.hi.y = 22;
	b.pts[0].x = 5;
	b.pts[1] = b.hi;
	print area(b); print " "; print b.pts[1].y; print "\n";
	bs[1] = b;
	bs[0].lo = b.hi;
	print bs[0].lo.y; print " "; print bs[1].pts[0].x; print "\n";
	p.x = 7; p.y = 8;
	if (p.x > 3) q = p;
	else q = b.lo;
	print q.y; print "\n";
	i = 0;
	while (i < 2)
	begin
		Pt t;
		t = copia(p);
		t.x = t.x + i;
		p = t;
		i = i + 1;
	end
	print p.x; print "\n";
	azzera(b.hi);
	print b.hi.x; print "\n";
end

void riempi(ref V v, int k)
begin
	int i;
	i = 0;
	v.n = k;
	while (i < 3)
	begin
		v.d[i] = k * (i + 1);
		i = i + 1;
	end
end

int tot(ref V v)
begin
	return v.d[0] + v.d[1] + v.d[2] + v.n;
end

int area(ref Box b)
begin
	return (b.hi.x - b.lo.x) * (b.hi.y - b.lo.y);
end

Pt copia(Pt a)
begin
	return a;
end

void azzera(ref Pt p)
begin
	p.x = 0;
end
