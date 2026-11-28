// "implementation" file: function definitions; include it after the main block
#include "test_inc_geom.inc"   // already included by the main file: ignored (once-only)

int quadrato(int n)
begin
	return n * n;
end

int dist2(Punto a, Punto b)
begin
	return quadrato(a.x - b.x) + quadrato(a.y - b.y);
end

int somma3(int a, int b, int c)
begin
	return a + b + c;
end
