#include "../include/utils.h"
#include "../include/graphics.h"
#include "../include/shader.h"

int main(){
    Engine rayTracingEngine;
    rayTracingEngine.prepareData();
    rayTracingEngine.render();
    return 0;
}
