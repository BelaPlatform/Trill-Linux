public class TrillTouch {
	float scale = 0.25;
	color touchColor = color(255, 204, 0);
	float size = 0.0;
	float location = { 0.0, 0.0 };
	boolean active = false;

	public TrillTouch(float scale, color touchColor, float size, float [] location, boolean active) {
		this.scale = scale;
		this.size = size;
		setLocation(location);
		this.active = active;
		this.touchColor = touchColor; 
	}

	public TrillTouch(float scale, color touchColor, float size, float [] location ) {
		this(scale, touchColor, size, location, false);
	}	 

	public void update(float [] location, float size) {
		this.active = true;
		setLocation(location);
		this.size = constrain(size, 0.0, 1.0);
	}

	public void setLocation(float [] location) {
		if(location.length != 2)
			throw new IllegalArgumentException("Location should be specified as coordinates in a 2D space.");
		for(int i = 0; i < this.location.length; i++) {
			 this.location[i] = location[i];
		}
	}	

	public void changeColor(color newColor) { this.color = newColor; }

	public void changeScale(float scale) {
		if(scale <= 1.0 ) {
			this.scale = scale;
		}
	}

	public boolean isActive() { return this.active; }
}
