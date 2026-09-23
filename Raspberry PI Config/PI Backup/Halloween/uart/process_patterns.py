import json

# Open and read the JSON file
with open('patterns.json', 'r') as file:
    data = json.load(file)

updated = {}

for key in data:
    if not any(chr.isdigit() for chr in key): updated[key] = data[key]

with open("patterns_final.json", "w") as outfile: 
	json.dump(updated, outfile)