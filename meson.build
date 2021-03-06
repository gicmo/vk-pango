project('vk-pango', 'c')

compiler = meson.get_compiler('c')

cairo    = dependency('cairo')
glib     = dependency('glib-2.0')
graphene = dependency('graphene-1.0')
vulkan   = dependency('vulkan')
glfw3    = dependency('glfw3')
pango    = dependency('pango')
pc       = dependency('pangocairo')
gtk4     = dependency('gtk+-4.0')

alldep = [cairo, glib, gtk4, glfw3, graphene, pango, vulkan, pc]

c_flags   = ['-Wall', '-Wunreachable-code']
ld_flags  = []

shaders = ['color.frag', 'box.vert']
compiled_shaders = []

glslc = find_program('glslc', required: true)

foreach shader: shaders
  basefn = shader.split('.').get(0)
  suffix = shader.split('.').get(1)

  stage_arg = suffix == 'frag' ? '-fshader-stage=fragment' : '-fshader-stage=vertex'
  spv_shader = '@0@.@1@.spv'.format(basefn, suffix)

  compiled = custom_target(spv_shader,
                           input: shader,
                           output: spv_shader,
                           command: [
                             glslc,
                             stage_arg,
                             '-DCLIP_NONE',
                             '-o', '@OUTPUT@',
			     '@INPUT@'
			   ])

  compiled_shaders += compiled

endforeach

tests = [
  ['testbinpacker', ['gbinpacker.c']]
]

foreach t: tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name), t.get(1, [])]
  executable(test_name, test_srcs,
	     cpp_args: c_flags,
	     link_args: ld_flags,
             dependencies: [cairo, glib, pango, pc])
endforeach

executable('vkpg',
	   sources: [['main.c',
		      'gbinpacker.h', 'gbinpacker.c'],
		     compiled_shaders],
	   cpp_args: c_flags,
	   link_args: ld_flags,
	   dependencies: alldep)

executable('gvkpg',
	   sources: [['win.c',
		      'gbinpacker.h', 'gbinpacker.c'],
		     compiled_shaders],
	   cpp_args: c_flags,
	   link_args: ld_flags,
	   dependencies: alldep)
