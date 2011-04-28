import os, math

def remove_decimal(value):
    return str(value).split('.')[0]

def fix_coordinates(file_name):
    current_dir = os.getcwd() 
    
    # We find and read the data we are going to process
    open_file_name = os.path.join(current_dir, "Raw Motion", file_name + ".movements")
    file_open = open(open_file_name, 'r')
    lines = file_open.readlines()
    file_open.close()

    to_write = "set ns [new Simulator]\n"
    id = 0

    # I create the nodes
    while(id < len(lines)):
        id += 1        
        to_write += "$ns node" + str(id) + "\n"
        
    to_write += "\n"

    id = 0
    # In this loop we take the movements for each node and turns it into data that can be understood by the program
    for line in lines:
        id += 1
        split_line = line.split(' ')
        # For each location there are 3 values -- time x_coor y_coor . Thus we know that split_line must contain a multiplum of 3 items 
        split_line_index = 0

        node_name = "$node" + str(id)

        # I set the intial address
        to_write += node_name + " set X_ " + str(split_line[1]) + "\n"
        to_write += node_name + " set Y_ " + str(split_line[2]) + "\n"
        to_write += node_name + " set Z_ 0\n\n"       

        while((split_line_index + 1) * 3 < len(split_line)):

            current_index = split_line_index * 3
            next_index = (split_line_index + 1) * 3
            
            time_to_move = float(split_line[current_index])
            time_to_arrive = float(split_line[next_index])

            x_from = float(split_line[current_index + 1])
            y_from = float(split_line[current_index + 2])

            x_to = float(split_line[next_index + 1])
            y_to = float(split_line[next_index + 2])

            x_delta = math.fabs(x_from - x_to)
            y_delta = math.fabs(y_from - y_to)

            speed = math.sqrt(x_delta * x_delta + y_delta * y_delta) / (time_to_arrive - time_to_move)

            to_write += "$ns at " + remove_decimal(time_to_move) + " " + node_name + " setdest " + str(x_to) + " " + str(y_to) + " " + str(speed) + "\n"

            split_line_index += 1
        to_write += "\n\n"
    
    # We write the data we have processed
    save_file_name = os.path.join(current_dir, "Processed Motion", file_name)
    file_write = open(save_file_name, 'w')
    file_write.write(to_write)
    file_write.close()


fix_coordinates("scenario1")
