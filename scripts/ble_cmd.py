import asyncio
from bleak import BleakScanner, BleakClient

DEV_NAME = "BeanOS"

SERVICE = "2b771fcc-c87d-42bd-9216-000000000000"
CTRL_RX = "2b771fcc-c87d-42bd-9216-000000000020"
CTRL_TX = "2b771fcc-c87d-42bd-9216-000000000010"
# DATA_RX = "12345678-0003-1000-8000-00805f9b34fb"
# DATA_TX = "12345678-0004-1000-8000-00805f9b34fb"

# Global variable to store the response                                                                   
response_data = None                                                                                      
                                                                                                          
def notification_handler(sender, data):                                                                   
    global response_data                                                                                  
    response_data = data.decode('utf-8').strip()                                                          
    print(f"Received response: {response_data[:50]}...")  # Show first 50 chars                           
                                                                                                          
async def find_device_by_name(name: str):                                                                 
    devices = await BleakScanner.discover(timeout=5.0)                                                    
    for d in devices:                                                                                     
        if d.name == name:                                                                                
            return d                                                                                      
    return None                                                                                           
                                                                                                          
async def send_command_and_wait(client, command, timeout=5.0):                                            
    global response_data                                                                                  
    response_data = None                                                                                  
                                                                                                          
    print(f"Sending command: {command}")                                                                  
    await client.write_gatt_char(CTRL_RX, command.encode('utf-8'), response=False)                        
                                                                                                          
    # Wait for response                                                                                   
    elapsed = 0.0                                                                                         
    while response_data is None and elapsed < timeout:                                                    
        await asyncio.sleep(0.1)                                                                          
        elapsed += 0.1                                                                                    
                                                                                                          
    return response_data                                                                                  
                                                                                                          
def hex_string_to_bytes(hex_string):                                                                      
    """Convert hex string to bytes"""                                                                     
    try:                                                                                                  
        return bytes.fromhex(hex_string)                                                                  
    except ValueError:                                                                                    
        return None                                                                                       
                                                                                                          
async def main():                                                                                         
    global response_data                                                                                  
                                                                                                          
    dev = await find_device_by_name(DEV_NAME)                                                             
    if not dev:                                                                                           
        raise SystemExit(f"Device named {DEV_NAME!r} not found. Make sure it's advertising.")             
                                                                                                          
    async with BleakClient(dev) as client:                                                                
        # Subscribe first (enables CCCD)                                                                  
        await client.start_notify(CTRL_TX, notification_handler)                                          
                                                                                                          
        # Get file stats first                                                                            
        stats_response = await send_command_and_wait(client, "data_stats")                                
        if not stats_response:                                                                            
            print("Failed to get data stats")                                                             
            return                                                                                        
                                                                                                          
        try:                                                                                              
            file_name, byte_size_str = stats_response.split(',')                                          
            total_bytes = int(byte_size_str)                                                              
            print(f"File: {file_name}, Total size: {total_bytes} bytes")                                  
        except ValueError:                                                                                
            print(f"Unexpected stats response format: {stats_response}")                                  
            return                                                                                        
                                                                                                          
        # Create output filename                                                                          
        output_filename = f"downloaded_{file_name}"                                                       
                                                                                                          
        # Read data in chunks                                                                             
        chunk_size = 512  # Should match max_read_size in the C code                                      
        offset = 0                                                                                        
                                                                                                          
        with open(output_filename, 'wb') as output_file:                                                  
            while offset < total_bytes:                                                                   
                # Send data_bytes command with offset                                                     
                cmd = f"data_bytes,{offset}"                                                              
                data_response = await send_command_and_wait(client, cmd, timeout=10.0)                    
                                                                                                          
                if not data_response:                                                                     
                    print(f"No response for offset {offset}")                                             
                    break                                                                                 
                                                                                                          
                if data_response.startswith("error") or data_response == "memory_error":                  
                    print(f"Error at offset {offset}: {data_response}")                                   
                    break                                                                                 
                                                                                                          
                # Convert hex string to bytes                                                             
                chunk_bytes = hex_string_to_bytes(data_response)                                          
                if chunk_bytes is None:                                                                   
                    print(f"Failed to parse hex data at offset {offset}")                                 
                    break                                                                                 
                                                                                                          
                # Calculate how many bytes to actually write (don't write beyond file size)               
                bytes_to_write = min(len(chunk_bytes), total_bytes - offset)                              
                output_file.write(chunk_bytes[:bytes_to_write])                                           
                                                                                                          
                offset += bytes_to_write                                                                  
                progress = (offset / total_bytes) * 100                                                   
                print(f"Progress: {offset}/{total_bytes} bytes ({progress:.1f}%)")                        
                                                                                                          
                # If we've read all the data, break                                                       
                if offset >= total_bytes:                                                                 
                    break                                                                                 
                                                                                                          
        print(f"Download complete! Data saved to: {output_filename}")                                     
        print(f"Total bytes downloaded: {offset}")                                                        
                                                                                                          
asyncio.run(main())                                                                                       
