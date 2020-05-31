public class TrillClass {
	String type;
	float [] dimensions = { 0.0, 0.0 };
	float cornerRadius = 0;
	float position[] = { 0.0, 0.0 };
	List<String> types = Arrays.asList("bar", "square", "hex", "ring");
	float touchScale = 0.4;
	color sensorColor = #000000; // black
	color[] touchColors = {#FF0000, #0000FF, #FFFF00, #FFFFFF, #00FFFF}; // red, blue, yellow, white, cyan
	ArrayList<TrillTouch> trillTouches = new ArrayList<TrillTouch>();

	public TrillClass(String type, float length, float [] position, float touchScale) {
		this.type = type.toLowerCase();
		if(types.contains(this.type)) {
			this.type = "unknown";
			throw new IllegalArgumentException("Unknown Trill type.");
		}
		resize(length);
		setPosition(position);
	}
	
	public static void setPosition(float [] position) {
		if(position.length != 2)
			throw new IllegalArgumentException("Position should be specified as coordinates in a 2D space.");
		for(int i = 0; i < this.position.length; i++) {
			 this.position[i] = position[i];
		}
	}

	public static void resize (float length) {

		this.dimensions[0] = length;
		if(this.type == "bar") {
			this.dimensions[1] = length/5;
		} if(this.type == "hex") {
			this.dimensions[1] = length/0.866;
		} else {
			this.dimensions[1] = length;
		}

		if(this.type == "bar") {
			this.cornerRadius = 0.03 * this.dimensions[0];
		} if(this.type == "square") {
			this.cornerRadius = 0.02 * this.dimensions[0];
		} else {
			this.cornerRadius = 0.0;
		}
	}

	public static void updateTouch (int i, float [] location, float size) {
	}
}
