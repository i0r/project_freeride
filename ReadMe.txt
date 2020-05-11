Dusk - Work In Progress Toy Engine (D3D11/D3D12/Vulkan)

TodoList
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
[Rendering]
	<Low>
		- Support multiscreens
			* Provide origin coordinates as parameter (x,y); create the windows with the specified origin
			* Use GetMonitorInfo( MonitorFromWindow( surface->Handle, MONITOR_DEFAULTTOPRIMARY ), &mi );
			  to figure out which monitor is the closest to the app rect (x, y, x + width, y + height rectangle)
			* Use the same monitor handle when creating the render device
		- Implement NVAPI
		- Implement AMD AGS
		- RenderDoc : support partial capture
			* Ex: MaPasseSuperCool->captureToRenderDoc() => DuskEngine_Direct3D11_MaPasseSuperCool_frame0.rdc
        - Framegraph: support dynamic sampler state
        - Custom image view/ buffer view support (during resource binding; view creation is done at resource creation time)
	<Medium>
		- Allow dynamic resize of the window
		- Allow refresh rate selection
			* e.g. "RefreshRate = 60 ; in Hz" in envars file
			* If the refresh rate is unavailable on the current monitor, fallback to the safest one (the lowest one?)
		- Abstract: manage cached PSO states
			* Should be loaded and created asynchronously
			* Use PSO Desc key hashcode to figure out binary cache name
			* Creating a manager dedicated to pso cache management should work
	<High>
		- D3D12 Backend Implementation (in progress)
			* Buffer: dynamic buffer usage (use volatile buffer; one huge buffer with proper buffer binding offset)
			* ResourceList: Cover missing usage cases
			* Misc: check for resource leak
			* Resources: implement descriptor caching for permanent resources
			* Descriptor Heaps: implement allocation strategies per resource usage and resource type
		- Vulkan Backend Implementation (in progress)
		- Make everything more data-driven!
            * Create a dedicated lexer for a dedicated fileformat
          * Bake images: Convert bitmaps to a compressed file format (DDS, BC6/BC7 depending on the channels and texture usage)
          * Bake geometry
            - .obj/.fbx/etc. to custom fileformat with preformated buffers for each render scenario (e.g. z prepass, gbuffer pass, etc.)
            - (not sure) try to parse material to make the workflow easier (or create placeholder material and let the user create his materials in editor?)
          * Bake cached resources
            - e.g. texture LUTs, convolution kernels, etc.
      
[Graphics]
	<Todo>
        - Bloom Unfuckery (single dispatch/better wavefront occupency; or even moving back to the gfx pipeline?)
		- Point light shadows (paraboloid)
		- Interior mapping
		- SSR
		- Realistic water/fluid simulation
		- Weather / forcast (see SebLagarde article & volumetric clouds weather map)
		- AntiAliased Wireframe
			* Need geometry shader support first!
		- Better Ubershader
			* Use multiscattering diffuse model (see Ubisoft Siggraph pres.)
			* Keep the whole stuff cheaper
			* Use tessellation by default
		- Color grading (using 3d LUT)
		- Re-implement volumetric clouds (optimize and proper editor impl; detail this later on)
		- Implement better sky model (should be able to recompute LUT in amortized/async/realtime)
		- Full asynchronous postfx pipeline
		- Screenshot feature

[Debug]
	<Todo>
		- GPU Profiler (à la PIX)
			* Frame timeline
			* A renderpass = a square
			* Hover the square to get exec. time & moar infos!
		- CPU Profiler
			* Tab setup: marker name, min, max, avg
			
[Core]
	<Todo>
		- Robust logger
			* Thread safe
			* Asynchronous
			* Support color (see task above)
			* Support category filtering (category is already setup, filtering is still left todo)		
		- Low level i/o (_open, _write, _read, etc.; drop anything from stdlib)
		- Better dbghelper library loading
		- Add color and formating for text parsing
			* See idTech/UE4 tech (e.g. "[color=#ff0000]MyRedText[/color]")
			* Should be standalone to be reused ingame/in editor/ in debug tools
			
[Physics]
	<Todo>
		- Bullet physics impl		
