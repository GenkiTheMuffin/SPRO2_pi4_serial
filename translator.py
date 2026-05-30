import sys
import csv


STUD_HEIGHT = 9.6 
STUD_TO_STUD = 8


# Reference positions 
PICK_UP_POSITION = [316,25,-1]
ORIGIN_2x2_POSITION = [269,24 ,-4.5]
ORIGIN_2x2_ROTATED_POSITION = [256,26 ,-4.5]

def main():
    with open('Instructions.csv', newline='') as instructions, \
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
    local_pick_up_position =   [PICK_UP_POSITION[0],PICK_UP_POSITION[1]+type_offset(type_),PICK_UP_POSITION[2]]
    
  

    #hover over pickup location
    pick_up_brick(local_pick_up_position, gcode_file)
    place_brick(type_,x,y,z,r,gcode_file)


def pick_up_brick(loc_pos, gcode_file):
    gcode_file.write(';Pick up brick\n')
    gcode_file.write('ROTATE_HOME\n')
    gcode_file.write(f'G1 Z{loc_pos[2]+2*STUD_HEIGHT}\n')
    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]+2*STUD_HEIGHT}\n')
    gcode_file.write('OPEN_JAW\n')
    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]}\n')
    gcode_file.write('G4 P1500\n')

    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]-1}\n')
    gcode_file.write('CLOSE_JAW\n')

    gcode_file.write(f'G1 X{loc_pos[0]} Y{loc_pos[1]} Z{loc_pos[2]+2*STUD_HEIGHT}\n\n')


def place_brick(type_,x,y,z,r,gcode_file):
    X = ORIGIN_2x2_POSITION[0] - x*STUD_TO_STUD
    Y = ORIGIN_2x2_POSITION[1] + y*STUD_TO_STUD + type_offset(type_)
    Z = ORIGIN_2x2_POSITION[2] + z*STUD_HEIGHT

    
    gcode_file.write(';Place brick\n')
    gcode_file.write(f'G1 Z{Z+2*STUD_HEIGHT}\n')
    gcode_file.write(f'G1 X{X} Y{Y} Z{Z+2*STUD_HEIGHT}\n')
    gcode_file.write(f'G1 X{X} Y{Y} Z{Z} F{100}\n')
    gcode_file.write('G4 P2000\n')
    
    gcode_file.write(f'G1 X{X} Y{Y} Z{Z+3}\n')
    gcode_file.write('G4 P800\n')
    gcode_file.write('OPEN_JAW\n\n')

    gcode_file.write(f'G1 X{X} Y{Y} Z{Z+2*STUD_HEIGHT} F1500\n')
    gcode_file.write(f'G1 X{PICK_UP_POSITION[0]} Y{PICK_UP_POSITION[1]}\n\n')
    

def type_offset(type_):
    match type_:
        case "2x2":
            return 0
        case "2x4":
            return 8
        case "2x6":
            return 16
        # If an exact match is not confirmed, this last case will be used if provided
        case _:
            print("Error: Not supported brick size")



if __name__ == '__main__':
    main()
