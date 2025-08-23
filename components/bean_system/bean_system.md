This is a leftover component that after splitting up all the functionality into it's own components, only performs the init of the i2c bus. This component will be deleted in the future.

There are only 2 i2c devices on the bus. Both have their driver in a seperate component.
Using the new esp-idf i2c_master.h it is possible to have both components querry the status of the i2c handle with `i2c_master_get_bus_handle()` and initialize themselves if needed.