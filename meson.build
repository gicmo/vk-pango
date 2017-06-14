project('vk-pango', 'c')

compiler = meson.get_compiler('c')

glib     = dependency('glib-2.0')
graphene = dependency('graphene-1.0')
vulkan   = dependency('vulkan')
glfw3    = dependency('glfw3')

alldep = [glib, glfw3, graphene, vulkan]

c_flags   = ['-Wall', '-Wunreachable-code']
ld_flags  = []

executable('vkpg',
	   sources: [ 'main.c' ],
	   cpp_args: c_flags,
	   link_args: ld_flags,
	   dependencies: alldep)
