# GDScript: world.gd

extends Node2D

# Godot AI Bridge (GAB) Variables
onready var gab = $GabLib  # library reference
onready var gab_options = {} 

func _ready():
	
	gab.connect(gab_options)

func _process(_delta):
	for agent in $Agents.get_children():
		_publish_state(agent)

### publishes the agent's current state to all clients ###
func _publish_state(agent):
	gab.send('/demo/agent/%s' % agent.id, agent.get_state())

### signal handler for the "event_requested" signal
func _on_event_requested(event_details):
	print('event requested ----> "%s"' % event_details)

	for agent in $Agents.get_children():
		if event_details['agent_id'] == agent.id:
			agent.add_action(event_details['action'])
