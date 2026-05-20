import sys
import csv


STUD_HEIGHT = 9.6 
STUD_TO_STUD = 8 

# Reference positions TBD
PICK_UP_POSITION = [280,40,0]
ORIGIN_STUD_POSITION = [256,4,0]


def main():
    with open('New_instructions.csv', newline='') as instructions, \
        open('output.gcode', 'w') as gcode_file:
        
        #initialise gcode file
        init_gcode(gcode_file)
        

        reader = csv.DictReader(instructions)  # each row as a dict
        for row in reader:
            parse_csv(row, gcode_file)



#initialise gcode file
def init_gcode(gcode_file):
    gcode_file.write(';Initialise machine\n')    
    gcode_file.write('G21 ; set units to mm \n')
    gcode_file.write('G90 ; absolute positioning\n')
    gcode_file.write('G92 X0 Y0 Z0 ; set current position\n')
    gcode_file.write('JAW_ROTATE ANGLE=0\n\n')

def parse_csv(row, gcode_file):
    #SUDO:
    #Find pick up location based on brick type
    #hover over pickup location
    #pick up
    #Go up to currentbuild layer+2
    #If rotate needed: TODO
        #different XY needed
    #Go to XY of place location    
    #Go down place 
    #Go up and away from plate

    type_ = row['Type']
    x = int(row['X'])
    y = int(row['Y'])
    z = int(row['Z'])
    r = bool(row['R'])
    
    #Finding pick up location based on brick type
    local_pick_up_position = find_local_pick_up_pos(type_)
    
    #hover over pickup location
    pick_up_brick(local_pick_up_position, gcode_file)
    place_brick(x,y,z,gcode_file)


#To find the pick up location based on the brick type
def find_local_pick_up_pos(type_):
    match type_:
        case "2x1":
            return PICK_UP_POSITION + [0,2,0]
        case "2x2":
            return PICK_UP_POSITION + [0,4,0]
        case "2x4":
            return PICK_UP_POSITION + [0,8,0]
        case "2x6":
            return PICK_UP_POSITION + [0,12,0]
        # If an exact match is not confirmed, this last case will be used if provided
        case _:
            print("Not supported brick size")


def pick_up_brick(loc_pos, gcode_file):
    gcode_file.write(';Pick up brick\n')
    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]+2*STUD_HEIGHT}\n')
    gcode_file.write('OPEN_JAW\n')
    
    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]}\n')
    gcode_file.write('CLOSE_JAW\n')

    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]+2*STUD_HEIGHT}\n\n')


def place_brick(x,y,z,gcode_file):
    X = ORIGIN_STUD_POSITION[0] - x*STUD_TO_STUD
    Y = ORIGIN_STUD_POSITION[1] + y*STUD_TO_STUD   
    Z = ORIGIN_STUD_POSITION[2] + z*STUD_HEIGHT
    gcode_file.write(';Place brick\n')
    gcode_file.write(f'G1 X{X} Y{Y} Z{Z+2*STUD_HEIGHT}\n')
    gcode_file.write(f'G1 X{X} Y{Y} Z{Z}\n')
    gcode_file.write('OPEN_JAW\n\n')

    gcode_file.write(f'G1 X{X} Y{Y} Z{Z+2*STUD_HEIGHT}\n')
    gcode_file.write(f'G1 X{PICK_UP_POSITION[0]} Y{PICK_UP_POSITION[1]}\n\n')
    




if _name_ == '_main_':
    main()