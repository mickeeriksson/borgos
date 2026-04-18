

//extern void hal_init_early(void);

/*
    When kmain is called.
        * log_init has been called to set *log_putchar_ptr function pointer
        *
 */
void kmain(void){

    //hal_init_early();

    int i=0;
    while(1){
        i+=1;
        //HANG HERE
    }
}