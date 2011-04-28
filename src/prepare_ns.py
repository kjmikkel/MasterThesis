import os

def remove_decimal(value):
    return str(value).split('.')[0]

def fix_coordinates(file_name):
    current_dir = os.getcwd() 
    
    # We find and read the data we are going to process
    open_file_name = os.path.join(current_dir, "Raw Motion", file_name + ".movements")
    file_open = open(open_file_name, 'r')
    lines = file_open.readlines()
    file_open.close()

    to_write = ""
    id = 0
    # In this loop we take the agent for each node and turns it into data that can be understood by the program
    for line in lines:
        id += 1
        split_line = line.split(' ')
        # For each location there are 3 values -- time x_coor y_coor . Thus we know that split_line must contain a multiplum of 3 items 
        split_line_index = 0
        while(split_line_index * 3 < len(split_line)):
            current_index = split_line_index * 3
            to_write += str(id) + ": " + remove_decimal(split_line[current_index]) + " " + remove_decimal(split_line[current_index + 1]) + " " + remove_decimal(split_line[current_index + 2]) + "\n" 

            split_line_index += 1
        print (split_line_index - 1)
        to_write += "\n\n"
    
    # We write the data we have processed
    save_file_name = os.path.join(current_dir, "Processed Motion", file_name)
    file_write = open(save_file_name, 'w')
    file_write.write(to_write)
    file_write.close()


fix_coordinates("scenario1")
