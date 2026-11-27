struct Punto { int x; real y; };
struct Seg { Punto a; Punto b; string nome; };

Punto mid(Punto p, Punto q);
void sposta(ref Punto p, int dx);
int somma(array v[] of Punto, int n);

Punto g;
array tab[4] of Punto;

begin
	Punto p, q, r;
	Seg s;
	array w[3] of Punto;
	int i;
	p.x = 3;
	p.y = 1.5;
	q = p;
	q.x = q.x + 10;
	print p.x; print " "; print q.x; print "\n";
	r = mid(p, q);
	print r.x; print " "; print r.y; print "\n";
	sposta(r, 5);
	print r.x; print "\n";
	s.a = p;
	s.b = q;
	s.nome = "seg";
	print s.nome; print " "; print s.b.x; print " "; print s.a.y; print "\n";
	i = 0;
	while (i < 3)
	begin
		w[i].x = i * 2;
		w[i].y = i + 0.5;
		i = i + 1;
	end
	w[1] = p;
	print somma(w, 3); print "\n";
	tab[2] = q;
	g = tab[2];
	print g.x; print " "; print w[1].y; print "\n";
end

Punto mid(Punto p, Punto q)
begin
	Punto r;
	r.x = (p.x + q.x) / 2;
	r.y = (p.y + q.y) / 2.0;
	return r;
end

void sposta(ref Punto p, int dx)
begin
	p.x = p.x + dx;
end

int somma(array v[] of Punto, int n)
begin
	int i;
	int s;
	s = 0;
	i = 0;
	while (i < n)
	begin
		s = s + v[i].x;
		i = i + 1;
	end
	return s;
end
