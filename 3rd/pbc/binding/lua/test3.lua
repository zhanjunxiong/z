protobuf = require "protobuf"
parser = require "parser"

t = parser.register("addressbook.proto","../../test")

optaddressbook = {
	person =  {
		name = "Alice",
		id = 12345,
		phone = {
			{ number = "1301234567" },
			{ number = "87654321", type = "WORK" },
		},
	}
}

code = protobuf.encode("tutorial.OptAddressBook", optaddressbook)

decode = protobuf.decode("tutorial.OptAddressBook" , code)

print(type(decode.person))
print(decode.person.name)
print(decode.person.id)

for _,v in ipairs(decode.person.phone) do
	print("\t"..v.number, v.type)
end


