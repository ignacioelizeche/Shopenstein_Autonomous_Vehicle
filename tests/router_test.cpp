// Pruebas del Router (sin hardware). Compilar y ejecutar con:
//   tests/run_routing_tests.sh
#include <cstdio>
#include <cstring>

#include "../esp32_wifi_bridge/router.h"

static int g_failures = 0;

static void check(bool ok, const char *what) {
  if (!ok) {
    ++g_failures;
    printf("FAIL: %s\n", what);
  }
}

static void expectHop(Router &r, char hop, const char *what) {
  check(r.peekNextHop() == hop, what);
}

static void expectPath(Router &r, const char *path, const char *what) {
  check(strcmp(r.remainingPath(), path) == 0, what);
}

int main() {
  // 1. Sin objetivos no hay ruta ni salto.
  {
    Router r(4, 0);
    check(r.currentLetter() == 'A', "arranca en A");
    check(r.peekNextHop() == '-', "sin objetivos no hay salto");
    expectPath(r, "", "sin objetivos no hay camino");
    check(r.plannedCost() == 0, "sin objetivos el costo es 0");
    check(r.targetCount() == 0, "sin objetivos");
  }

  // 2. Un objetivo: A->D se expande por aristas y cierra en E.
  {
    Router r(4, 0);
    check(r.addTarget(3), "agrega D");
    expectHop(r, 'B', "primer salto A->D es B");
    expectPath(r, "ABDE", "camino A->B->D->E");
    check(r.plannedCost() == 7, "costo A->B->D->E = 7");
    check(strcmp(r.targetsText(), "D") == 0, "objetivos: D");
  }

  // 3. Recorrido completo hasta la salida.
  {
    Router r(4, 0);
    r.addTarget(3);
    check(r.takeNextHop() == 'B', "entrega B");
    r.setCurrent(1);                       // llegó a B
    expectHop(r, 'D', "desde B sigue D");
    expectPath(r, "BDE", "camino restante desde B");
    check(r.takeNextHop() == 'D', "entrega D");
    r.setCurrent(3);                       // llegó a D (objetivo cumplido)
    expectHop(r, 'E', "sin objetivos pendientes cierra en E");
    expectPath(r, "DE", "camino D->E");
    check(r.takeNextHop() == 'E', "entrega E");
    r.setCurrent(4);                       // llegó a la salida
    check(r.peekNextHop() == '-', "en E no hay más saltos");
    expectPath(r, "", "ruta terminada");
    check(strcmp(r.targetsText(), "") == 0, "sin objetivos pendientes");
    check(r.plannedCost() == 0, "costo final 0");
  }

  // 4. Varios objetivos: se reordena por costo (TSP), no por orden de toque.
  {
    Router r(4, 0);
    r.addTarget(3);  // D
    r.addTarget(1);  // B
    expectPath(r, "ABDE", "D+B se optimiza a A->B->D->E");
    check(r.plannedCost() == 7, "costo optimizado 7");
    check(strcmp(r.targetsText(), "DB") == 0, "se recuerda el pedido");
  }

  // 5. Agregar un objetivo en pleno salto no mueve la ruta bajo los pies.
  {
    Router r(4, 0);
    r.addTarget(3);
    check(r.takeNextHop() == 'B', "entrega B");
    r.addTarget(2);                        // C, mientras va a B
    expectHop(r, 'B', "sigue mostrando el salto en curso");
    expectPath(r, "ABDE", "la ruta no cambia hasta llegar");
    r.setCurrent(1);                       // llegó a B: replantea
    expectPath(r, "BCDE", "ruta replanteada desde B");
    check(r.plannedCost() == 6, "costo B->C->D->E = 6");
  }

  // 6. Corregir la posición replantea desde ahí y saca el nodo de objetivos.
  {
    Router r(4, 0);
    r.addTarget(3);
    r.setCurrent(1);                       // el usuario dice "estoy en B"
    expectPath(r, "BDE", "replantea desde B");
    check(r.targetCount() == 1, "D sigue pendiente");
  }

  // 7. Borrar la ruta cancela todo.
  {
    Router r(4, 0);
    r.addTarget(3);
    r.takeNextHop();
    r.clearTargets();
    check(r.peekNextHop() == '-', "tras borrar no hay salto");
    expectPath(r, "", "tras borrar no hay camino");
    check(r.takeNextHop() == '-', "no entrega saltos viejos");
  }

  // 8. Duplicados y capacidad.
  {
    Router r(4, 0);
    check(r.addTarget(3), "agrega D");
    check(!r.addTarget(3), "no duplica D");
    check(!r.addTarget(7), "rechaza nodo inválido");
    int added = 0;
    for (uint8_t n = 0; n < NODE_COUNT; ++n) {
      if (n != 3 && r.addTarget(n)) ++added;
    }
    check(r.targetCount() == NODE_COUNT, "acepta los 5 nodos distintos");
    check(!r.addTarget(0), "rechaza el sexto (duplicado)");
    check(added == 4, "solo agregó los 4 restantes");
  }

  // 9. Un solo objetivo que es la salida: A->B->E.
  {
    Router r(4, 0);
    r.addTarget(4);
    expectPath(r, "ABE", "A->E pasa por B");
    check(r.plannedCost() == 4, "costo A->E = 4");
  }

  if (g_failures == 0) {
    printf("OK: todas las pruebas del Router pasaron\n");
    return 0;
  }
  printf("%d prueba(s) fallaron\n", g_failures);
  return 1;
}
