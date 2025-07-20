#include "../include/utils.h"
#include "../include/shader.h"
#include "../include/graphics.h"

int main(){
    Engine rayTracingEngine;
    rayTracingEngine.prepareData();
    rayTracingEngine.render();
    return 0;
}
