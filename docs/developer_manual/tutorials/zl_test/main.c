#include <stdio.h>
#include <stdint.h>

#define DMA_BA   0x20000000
#define ACCEL_BA 0x30000000

#define DMA_WRITE(offset,val)  *(volatile uint32_t*)(DMA_BA + offset) = val
#define DMA_READ(offset)  *(volatile uint32_t*)(DMA_BA + offset)
#define ACCEL_WRITE(offset,val)  *(volatile uint32_t*)(ACCEL_BA + offset) = val
#define ACCEL_READ(offset)  *(volatile uint32_t*)(ACCEL_BA + offset)


int main()
{

    // int i=0;
    // int a=0;

    DMA_WRITE(0x0,0x30000000); //src
    DMA_WRITE(0x4,0x30000100); //dst
    DMA_WRITE(0x8,0x10);
    DMA_WRITE(0xC,0x1);


    // for(i=0;i<10000;i++) {
    //     a++;
    // }

    // printf("a is %d\n",a);
    
    //printf("mm_accel data is 0x%08x\n",ACCEL_READ(0x10));
    
    printf("DMA start reg val is 0x%08x\n",DMA_READ(0xC));

    

    //*(accel_ba+0x0)=0x10; //src

    return 0;
}
