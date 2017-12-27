/* pa3.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "classify.h"

//defines for file form
#define JPG_FORM 1
#define HTML_FORM 0

//create constant int file writing forms
const unsigned int jpg_clus_form = 0x67706a2e;
const unsigned int html_clus_form = 0x6d74682e;

//function decleration
int write_entry(int type, unsigned int cluster, int unsigned num,  FILE* map);
int increment_count(unsigned int update,  unsigned int compare, char *flag);

int main(int argc, char *argv[])
{
    

    int input_fd;
    int classification_fd;
    int cluster_number;
    int bytes_read;
    unsigned char classification, cluster_type;
    char cluster_data[CLUSTER_SIZE];
    
    // We only accept running with one command line argumet: the name of the
    // data file
    if (argc != 2) {
        printf("Usage: %s data_file\n", argv[0]);
        return 1;
    }
    
    // Try opening the file for reading, exit with appropriate error message
    // if open fails
    input_fd = open(argv[1], O_RDONLY);
    if (input_fd < 0) {
        printf("Error opening file \"%s\" for reading: %s\n", argv[1], strerror(errno));
        return 1;
    }
    
    // Iterate through all the clusters, reading their contents 
    // into cluster_data
    cluster_number = 0;
    FILE* class = fopen(CLASSIFICATION_FILE, "w");


    //Read one cluster at a time from the input file
    while ((bytes_read = read(input_fd, &cluster_data, CLUSTER_SIZE)) > 0) {
        assert(bytes_read == CLUSTER_SIZE);
	
	//set class byte back to 0
        classification = TYPE_UNCLASSIFIED;
        
	//check for jpg cluster
	if(is_jpg(cluster_data) || has_jpg_header(cluster_data) 
	|| has_jpg_footer(cluster_data))
	{
		classification |= TYPE_IS_JPG; //append type JPG to byte
		if(has_jpg_header(cluster_data)) //check if jpg is header
		{
			 //append type jpg header to class byte
			 classification |= TYPE_JPG_HEADER;
		}
		if(has_jpg_footer(cluster_data)) //check if jpg is footer
		{
			 //append type jpg footer to class byte
			 classification |= TYPE_JPG_FOOTER;
			
		}
		
		
	}
	//check for html cluster
	else if(is_html(cluster_data) || has_html_header(cluster_data) ||
		has_html_footer(cluster_data))
	{
		classification |= TYPE_IS_HTML; //append type html to byte
		if(has_html_header(cluster_data)) //check if html is header
		{
			//append type html header to class byte
			 classification |= TYPE_HTML_HEADER;
		}
		else if(has_html_footer(cluster_data)) //check if html is footer
		{
			//append type html footer to class byte
			 classification |= TYPE_HTML_FOOTER;
			
		}
	}
	else
	{
		//set byte to be undefined if can't be identified
		cluster_type = TYPE_UNCLASSIFIED;
	}
	//write class byte to classification file
	fwrite(&classification, sizeof(char), 1, class);
        /*
            In this loop, you need to implement the functionality of the
            classifier. Each cluster needs to be examined using the functions
            provided in classify.c. Then for each cluster, the attributes
            need to be written to the classification file.
        */
        
        printf("Processing cluster %06d\n", cluster_number); // This line needs to be removed in your final submission
        
	
	//increment cluster number
        cluster_number++;
    }
    
    close(input_fd);
    fclose(class);

    
    // Try opening the classification file for reading, exit with appropriate
    // error message if open fails 
    classification_fd = open(CLASSIFICATION_FILE, O_RDONLY); // Instead of opening this file here, you may elect to open it before the classifier loop in read/write mode
    if (classification_fd < 0) {
        printf("Error opening file \"%s\": %s\n", CLASSIFICATION_FILE, strerror(errno));
        return 1;
    }
    
    
    // Iterate over each cluster, reading in the cluster type attributes
    FILE *map = fopen(MAP_FILE, "w");
    char foundJpgFoot;
    char foundHtmlFoot;
    unsigned int jpg_cluster = 0;
    unsigned int html_cluster = 0;
    unsigned int count = 0;
    unsigned int html_count = 1;
    unsigned int jpg_count = 1;
    
    //read one byte at a time from class file
    while ((bytes_read = read(classification_fd, &cluster_type, 1)) > 0) {
	
	//check if cluster is type jpg
	if(cluster_type & TYPE_IS_JPG)
	{	
		//check if jpg footer was found on last iteration
		if(foundJpgFoot)  
		{
			//increment the count of jpg files
			jpg_count = increment_count(jpg_count, html_count, &foundJpgFoot);
		}
		if(!foundHtmlFoot && count > 0)//check if last html written was a footer and not first cluster
		{
			//write entry for lost byte on html file writing
			write_entry(JPG_FORM , jpg_cluster++, jpg_count, map);
		}
		write_entry(JPG_FORM , jpg_cluster++, jpg_count, map);//write entry for the current read byte
		
		//read bytes from file until an html cluster is found
		while((bytes_read = read(classification_fd, &cluster_type, 1)) > 0 
			&& (cluster_type & TYPE_IS_JPG))
		{	
			//continually write all subsequent jpg clusters while incrementing the cluster count
			write_entry(JPG_FORM , jpg_cluster++, jpg_count, map);
			
			//check if current cluster is a footer
			if(cluster_type & TYPE_JPG_FOOTER)
			{ 
				foundJpgFoot = 1; //set flag for jpg footer found
				jpg_cluster = 0; //reset cluster count to 0
				break;
			}
			
		}
		if(count == 0) //check if first cluster
		{
			html_count++; //increment html count
		}
	}
	else if(cluster_type & TYPE_IS_HTML) //check if cluster is html
	{	
		//check if html footer was found on last iteration
		if(foundHtmlFoot) 
		{
			//increment count of html files
			html_count = increment_count(html_count, jpg_count, &foundHtmlFoot);
		}
		if(!foundJpgFoot && count > 0)//check if last jpg written was a footer and not first cluster
		{
			//write entry for lost byte on html file writing
			write_entry(HTML_FORM, html_cluster++, html_count, map);
		}
		write_entry(HTML_FORM, html_cluster++, html_count, map); //write entry for current read byte

		//read clusters until jpg is found
		while((bytes_read = read(classification_fd, &cluster_type, 1)) > 0  
			&& (cluster_type & TYPE_IS_HTML))
		{	
			//continually write all subsequent html clusters while incrementing the cluster count
			write_entry(HTML_FORM, html_cluster++, html_count, map);
			
			//check if cluster is html footer
			if(cluster_type & TYPE_HTML_FOOTER)
			{ 
				foundHtmlFoot = 1; //set html footer flag
				html_cluster = 0; //reset cluster count
				break;
			}
		} 
		if(count == 0) //check if first cluster
		{
			jpg_count++; //increment jpg count
		}
	}
	count++; //increment cluster count
	
    }
    //close class file and map files
    close(classification_fd);
    fclose(map);
    return 0;
};

   
int write_entry(int type, unsigned int cluster, unsigned int num,  FILE* map)
{
	//create string to write file num
	char* file = calloc(sizeof(char), 8);
	sprintf(file, "file%04d", num);

	fwrite(file, sizeof(file), 1 , map); //write file num
	if(type == JPG_FORM)//check if writing jpg
	{
		fwrite(&jpg_clus_form, sizeof(int), 1 , map); //write jpg cluster form
	}
	else
	{
		fwrite(&html_clus_form, sizeof(int), 1 , map);	//write html cluster form
	}
	fwrite(&cluster, sizeof(int), 1 , map);	//write cluster
	free(file);//free buffer		
	return 0;
}

int increment_count(unsigned int update, unsigned int compare, char *flag)
{
	//check if comparing file count is 
	//greater than updating file count
	if(compare > update)
	{	
		//increment the update file count to be other file count + 1
		update = compare +1;
	}
	else
	{
		//or else increment by 1
		update++;
	}	
	//set footer flag to false
	*flag = 0;
return update;


}

