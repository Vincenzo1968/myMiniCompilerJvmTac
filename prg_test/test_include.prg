#include "test_inc_geom.inc"
#include "test_inc_geom.inc"

begin
	Punto p;
	Punto q;
	p.x = 1; p.y = 2;
	q.x = 4; q.y = 6;
	print dist2(p, q); print "\n";
	print somma3(quadrato(2), quadrato(3), 1); print "\n";
end

#include "test_inc_geom_impl.inc"
