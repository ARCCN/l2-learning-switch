from conans import ConanFile, tools

class L2LearningSwitchConan(ConanFile):
    name = "L2LearningSwitch"
    version = "0.1"
    settings = None
    description = "L2 Learning Switch"
    url = "None"
    license = "None"
    author = "None"
    topics = None

    def package(self):
        self.copy("*")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)